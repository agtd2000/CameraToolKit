"""
SCM 测试图像生成脚本
======================
基于 EMVA 1288 物理模型生成模拟传感器图像：
  - 暗场图像：暗电流 + 读出噪声（高斯）
  - 亮场图像：信号 + 暗电流 + 散粒噪声（泊松）+ PRNU（像素响应非均匀性）

用法：
    python generate_scm_test_images.py --bits 16 --exposures 1000,2000,4000 \
        --frames 50 --width 640 --height 480 \
        --dark-dir "E:/Projects/Study/CameraToolKit/images/dark" \
        --bright-dir "E:/Projects/Study/CameraToolKit/images/bright"
"""

import argparse
import os
import time
import numpy as np
import cv2


# ---------- 传感器物理参数（可调） ----------
class SensorModel:
    """模拟工业相机传感器的物理特性（EMVA 1288）"""
    def __init__(self, bits: int):
        self.bits = bits
        self.max_dn = (1 << bits) - 1          # 满量程 DN
        self.full_well_e = 30000.0             # 满阱容量 (e-)
        self.system_gain = 0.5                 # 系统增益 (e-/DN)，即 1 DN = 2 e-
        self.dark_current_e_per_s = 1000.0     # 暗电流 (e-/s)
        self.read_noise_e = 15.0               # 读出噪声 (e-, RMS)
        self.prnu_sigma = 0.01                 # PRNU 像素响应非均匀性 (1%)
        # 光强 (e-/s)：使最长曝光时亮场达到约 70% 满阱
        self.light_intensity_e_per_s = 5.0e6   # 5,000,000 e-/s
        # DC 偏置（黑电平）：真实相机为避免暗信号被截断到 0 而设置的偏置
        # 必须远大于读出噪声，确保暗场不被截断（EMVA 1288 标准做法）
        self.dark_offset_dn = 100.0            # 100 DN = 200 e-


def generate_dark_frame(sensor: SensorModel, exposure_us: float,
                        width: int, height: int, rng: np.random.Generator) -> np.ndarray:
    """生成一帧暗场图像（无光照，仅暗电流 + 读出噪声 + DC 偏置）"""
    exp_s = exposure_us * 1e-6
    # 暗电流信号 (e-)
    dark_signal_e = sensor.dark_current_e_per_s * exp_s
    # 转换为 DN
    dark_signal_dn = dark_signal_e / sensor.system_gain
    # 读出噪声 (e-) -> 高斯噪声 (DN)
    read_noise_dn = sensor.read_noise_e / sensor.system_gain
    noise = rng.normal(0.0, read_noise_dn, size=(height, width))
    # 合成：DC 偏置 + 暗信号 + 噪声，裁剪到有效范围
    img = sensor.dark_offset_dn + dark_signal_dn + noise
    img = np.clip(img, 0, sensor.max_dn)
    return img.astype(np.uint16 if sensor.bits > 8 else np.uint8)


def generate_bright_frame(sensor: SensorModel, exposure_us: float,
                          width: int, height: int, rng: np.random.Generator,
                          prnu_map: np.ndarray) -> np.ndarray:
    """生成一帧亮场图像（信号 + 暗电流 + 散粒噪声 + PRNU + DC 偏置）"""
    exp_s = exposure_us * 1e-6
    # 光信号 (e-)，乘以 PRNU 响应系数
    signal_e = sensor.light_intensity_e_per_s * exp_s * prnu_map
    # 散粒噪声（泊松）：用高斯近似加速（信号较大时成立）
    shot_noise_e = rng.normal(0.0, np.sqrt(np.maximum(signal_e, 0.0)))
    signal_with_shot_e = signal_e + shot_noise_e
    # 暗电流
    dark_signal_e = sensor.dark_current_e_per_s * exp_s
    # 读出噪声
    read_noise_e = rng.normal(0.0, sensor.read_noise_e, size=(height, width))
    # 合成总电子数
    total_e = signal_with_shot_e + dark_signal_e + read_noise_e
    # 转换为 DN，加上 DC 偏置
    img_dn = sensor.dark_offset_dn + total_e / sensor.system_gain
    img = np.clip(img_dn, 0, sensor.max_dn)
    return img.astype(np.uint16 if sensor.bits > 8 else np.uint8)


def save_image(img: np.ndarray, path: str, bits: int):
    """保存图像：16-bit 用 PNG 无损，8-bit 用 BMP"""
    if bits > 8:
        # 16-bit PNG 无损保存
        cv2.imwrite(path, img)
    else:
        # 8-bit BMP
        cv2.imwrite(path, img)


def generate_set(sensor: SensorModel, exposures: list, frames: int,
                 width: int, height: int, out_dir: str, is_dark: bool):
    """生成一组图像（多曝光时间，每曝光 N 帧）"""
    rng = np.random.default_rng(42 if is_dark else 1234)
    # PRNU 图（每个曝光时间共用一张，体现像素固定响应差异）
    prnu_map = 1.0 + rng.normal(0.0, sensor.prnu_sigma, size=(height, width))
    prnu_map = np.clip(prnu_map, 0.5, 1.5)

    total = 0
    t0 = time.time()
    for exp_us in exposures:
        # 子文件夹命名：exp_XXXXus
        sub_dir = os.path.join(out_dir, f"exp_{int(exp_us)}us")
        os.makedirs(sub_dir, exist_ok=True)

        for i in range(frames):
            if is_dark:
                img = generate_dark_frame(sensor, exp_us, width, height, rng)
            else:
                img = generate_bright_frame(sensor, exp_us, width, height, rng, prnu_map)

            # 文件命名：dark_1000us_0001.png / bright_2000us_0001.png
            prefix = "dark" if is_dark else "bright"
            ext = "png" if sensor.bits > 8 else "bmp"
            fname = f"{prefix}_{int(exp_us)}us_{i+1:04d}.{ext}"
            save_image(img, os.path.join(sub_dir, fname), sensor.bits)
            total += 1

            # 进度提示（每 10 帧）
            if (i + 1) % 10 == 0:
                print(f"  [{prefix}] exp={exp_us}us: {i+1}/{frames} frames")

    elapsed = time.time() - t0
    print(f"  [{ 'dark' if is_dark else 'bright' }] 完成: {total} 张, 用时 {elapsed:.1f}s")


def main():
    parser = argparse.ArgumentParser(description="生成 SCM 测试图像")
    parser.add_argument("--bits", type=int, default=16, choices=[8, 10, 12, 16],
                        help="位深 (8/10/12/16)")
    parser.add_argument("--exposures", type=str, default="1000,2000,4000",
                        help="曝光时间列表 (us)，逗号分隔")
    parser.add_argument("--frames", type=int, default=50,
                        help="每个曝光时间的帧数")
    parser.add_argument("--width", type=int, default=640, help="图像宽度")
    parser.add_argument("--height", type=int, default=480, help="图像高度")
    parser.add_argument("--dark-dir", type=str,
                        default="E:/Projects/Study/CameraToolKit/images/dark",
                        help="暗场图像输出目录")
    parser.add_argument("--bright-dir", type=str,
                        default="E:/Projects/Study/CameraToolKit/images/bright",
                        help="亮场图像输出目录")
    args = parser.parse_args()

    exposures = [float(x.strip()) for x in args.exposures.split(",")]
    sensor = SensorModel(bits=args.bits)

    print("=" * 60)
    print("SCM 测试图像生成器")
    print("=" * 60)
    print(f"  位深:        {args.bits}-bit (max DN = {sensor.max_dn})")
    print(f"  图像尺寸:    {args.width} x {args.height}")
    print(f"  曝光时间:    {exposures} us")
    print(f"  每组帧数:    {args.frames}")
    print(f"  暗场目录:    {args.dark_dir}")
    print(f"  亮场目录:    {args.bright_dir}")
    print(f"  系统增益:    {sensor.system_gain} e-/DN")
    print(f"  满阱容量:    {sensor.full_well_e} e-")
    print(f"  暗电流:      {sensor.dark_current_e_per_s} e-/s")
    print(f"  读出噪声:    {sensor.read_noise_e} e- (RMS)")
    print(f"  PRNU:        {sensor.prnu_sigma*100:.1f}%")
    print(f"  光强:        {sensor.light_intensity_e_per_s:.2e} e-/s")
    print(f"  DC偏置:      {sensor.dark_offset_dn} DN (避免暗信号截断)")
    print("-" * 60)
    print(f"预计输出: {len(exposures)*args.frames*2} 张图像")
    print("=" * 60)

    os.makedirs(args.dark_dir, exist_ok=True)
    os.makedirs(args.bright_dir, exist_ok=True)

    print("\n[1/2] 生成暗场图像...")
    generate_set(sensor, exposures, args.frames, args.width, args.height,
                 args.dark_dir, is_dark=True)

    print("\n[2/2] 生成亮场图像...")
    generate_set(sensor, exposures, args.frames, args.width, args.height,
                 args.bright_dir, is_dark=False)

    print("\n" + "=" * 60)
    print("全部完成！")
    print(f"  暗场: {args.dark_dir}")
    print(f"  亮场: {args.bright_dir}")
    print("=" * 60)
    print("\n使用说明：")
    print("  1. 在 SCM 面板填写 Exposure (us) = 例如 1000")
    print("  2. Dark Path 填写: <dark_dir>/exp_1000us")
    print("  3. Bright Path 填写: <bright_dir>/exp_1000us")
    print("  4. 点击 Load 加载该曝光时间下的 50 帧")
    print("  5. 更换曝光时间重复上述步骤（PTC 需要 ≥2 个曝光时间）")


if __name__ == "__main__":
    main()
