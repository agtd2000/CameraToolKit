"""
SCM 修复验证脚本
=================
用 IMREAD_UNCHANGED 读取 16-bit 图像，复现 SCM 评估核心逻辑，
验证修复后能得到合理的物理指标。
"""
import cv2
import numpy as np
from pathlib import Path

DARK_DIR = Path("images/dark")
BRIGHT_DIR = Path("images/bright")
EXPOSURES = [1000.0, 2000.0, 4000.0]  # us
BITS = 16
MAX_DN = (1 << BITS) - 1


def load_images(folder: Path, exp_us: float, prefix: str):
    """加载某曝光时间下的所有图像（IMREAD_UNCHANGED 保留 16-bit）"""
    sub_dir = folder / f"exp_{int(exp_us)}us"
    files = sorted(sub_dir.glob(f"{prefix}_{int(exp_us)}us_*.png"))
    imgs = []
    for f in files:
        img = cv2.imread(str(f), cv2.IMREAD_UNCHANGED)
        if img is not None:
            imgs.append(img.astype(np.float64))
    return imgs


def compute_mean_variance(images):
    """EMVA 1288 时间方差统计
    - 对每个像素位置计算多帧时间均值和时间方差
    - 所有像素取平均，可消除 PRNU 等固定模式噪声
    - 仅保留时间噪声（读出噪声 + 散粒噪声），使 PTC 拟合正确
    """
    if not images:
        return 0.0, 0.0
    stack = np.stack(images, axis=0)  # (N, H, W)
    mean_img = stack.mean(axis=0)
    var_img = stack.var(axis=0, ddof=1)  # 每像素时间方差
    out_mean = mean_img.mean()
    out_variance = var_img.mean()  # 时间方差的空间平均
    return out_mean, out_variance


def main():
    print("=" * 60)
    print("SCM 修复验证（使用 IMREAD_UNCHANGED 读取 16-bit）")
    print("=" * 60)

    # 1. 验证图像读取
    print("\n[1] 图像读取验证")
    img_test = cv2.imread(str(BRIGHT_DIR / "exp_1000us" / "bright_1000us_0001.png"),
                          cv2.IMREAD_UNCHANGED)
    print(f"  dtype={img_test.dtype}, shape={img_test.shape}, max={img_test.max()}")

    # 2. 计算各曝光时间的均值/方差
    print("\n[2] 各曝光时间统计")
    dark_stats = {}
    bright_stats = {}
    for exp in EXPOSURES:
        dark_imgs = load_images(DARK_DIR, exp, "dark")
        bright_imgs = load_images(BRIGHT_DIR, exp, "bright")
        d_mean, d_var = compute_mean_variance(dark_imgs)
        b_mean, b_var = compute_mean_variance(bright_imgs)
        dark_stats[exp] = (d_mean, d_var)
        bright_stats[exp] = (b_mean, b_var)
        print(f"  exp={exp:6.0f}us | dark: mean={d_mean:8.2f} var={d_var:10.2f} | "
              f"bright: mean={b_mean:8.2f} var={b_var:10.2f} | frames={len(dark_imgs)}/{len(bright_imgs)}")

    # 3. PTC 拟合：variance = system_gain * mean
    #    系统增益 = 斜率 = sum((mean_i - mean_avg)*(var_i - var_avg)) / sum((mean_i - mean_avg)^2)
    print("\n[3] PTC 拟合（variance vs mean）")
    means = []
    variances = []
    for exp in EXPOSURES:
        b_mean, b_var = bright_stats[exp]
        d_mean, d_var = dark_stats[exp]
        # 净信号 = 亮场 - 暗场
        net_mean = b_mean - d_mean
        net_var = b_var - d_var  # 净方差
        means.append(net_mean)
        variances.append(net_var)
        print(f"  exp={exp:6.0f}us | net_mean={net_mean:8.2f} net_var={net_var:10.2f}")

    means = np.array(means)
    variances = np.array(variances)
    # 线性拟合 var = gain * mean
    gain_dn_per_e = np.polyfit(means, variances, 1)[0]  # 斜率 = system_gain (DN/e- 实际是 var/mean)
    # 在 EMVA 1288 中: variance(DN^2) = (1/K) * mean(DN), K = e-/DN
    # 所以 system_gain_e_per_dn = mean / variance 的斜率倒数
    # 即 K = 1 / slope
    system_gain_e_per_dn = 1.0 / gain_dn_per_e if gain_dn_per_e > 0 else 0
    system_gain_dn_per_e = gain_dn_per_e
    print(f"\n  PTC 斜率 (var/mean) = {gain_dn_per_e:.6f}")
    print(f"  系统增益 K = {system_gain_e_per_dn:.4f} e-/DN  (期望: 0.5)")
    print(f"  系统增益 = {system_gain_dn_per_e:.4f} DN/e-  (期望: 2.0)")

    # 4. 暗噪声
    print("\n[4] 暗噪声计算（用最短曝光）")
    d_mean_short, d_var_short = dark_stats[EXPOSURES[0]]
    dark_noise_dn = np.sqrt(d_var_short)
    dark_noise_e = dark_noise_dn * system_gain_e_per_dn
    print(f"  暗场方差 = {d_var_short:.4f} DN^2")
    print(f"  暗噪声 = {dark_noise_dn:.4f} DN = {dark_noise_e:.4f} e-  (期望: 15 e-)")

    # 5. 暗电流
    print("\n[5] 暗电流计算")
    dark_signals = [dark_stats[exp][0] for exp in EXPOSURES]
    exp_s = np.array(EXPOSURES) * 1e-6
    # 暗信号 = dark_current_e_per_s * exp_s / K
    # 斜率 = dark_current_e_per_s / K
    slope_dn_per_s = np.polyfit(exp_s, dark_signals, 1)[0]
    dark_current_e_per_s = slope_dn_per_s * system_gain_e_per_dn
    print(f"  暗信号斜率 = {slope_dn_per_s:.4f} DN/s")
    print(f"  暗电流 = {dark_current_e_per_s:.4f} e-/s  (期望: 1000)")

    # 6. 满阱容量
    print("\n[6] 满阱容量")
    full_well_dn = MAX_DN
    full_well_e = full_well_dn * system_gain_e_per_dn
    print(f"  满阱 = {full_well_dn} DN = {full_well_e:.2f} e-  (期望: ~32767)")

    # 7. 动态范围
    print("\n[7] 动态范围")
    dr = full_well_e / dark_noise_e if dark_noise_e > 0 else 0
    dr_db = 20 * np.log10(dr) if dr > 0 else 0
    dr_bits = dr_db / 6.02
    print(f"  DR = {dr:.2f} = {dr_db:.2f} dB = {dr_bits:.2f} bit")

    # 8. 评估结论
    print("\n" + "=" * 60)
    print("评估结论")
    print("=" * 60)
    print(f"  系统增益: {system_gain_e_per_dn:.4f} e-/DN   (期望 0.5)")
    print(f"  暗噪声:   {dark_noise_e:.4f} e-            (期望 15)")
    print(f"  暗电流:   {dark_current_e_per_s:.4f} e-/s        (期望 1000)")
    print(f"  满阱:     {full_well_e:.2f} e-            (期望 ~32767)")
    print(f"  动态范围: {dr_db:.2f} dB ({dr_bits:.2f} bit)")
    is_valid = (system_gain_e_per_dn > 0 and full_well_e > 0 and dr_db > 0)
    print(f"  评估有效性: {'有效 ✓' if is_valid else '无效 ✗'}")
    print("=" * 60)


if __name__ == "__main__":
    main()
