import struct
import numpy as np

def generate_factory_calib_bin(output_path):
    """
    生成符合二进制格式的factory_calib.dat文件
    """
    # 定义文件头
    magic = 0x53504342  # "SPCB"
    version = 1
    num_sources = 3  # 3个光源
    mode_val = 0  # FACTORY
    complexity_val = 0  # STANDARD

    # 生成光源数据
    light_sources = []

    # 光源1: D65 (6500K)
    ls1 = {
        'name': 'D65',
        'color_temp': 6500.0,
        'curves': [generate_spectral_curve(6500)]
    }
    light_sources.append(ls1)

    # 光源2: D50 (5000K)
    ls2 = {
        'name': 'D50',
        'color_temp': 5000.0,
        'curves': [generate_spectral_curve(5000)]
    }
    light_sources.append(ls2)

    # 光源3: A光源 (2856K)
    ls3 = {
        'name': 'A-Light',
        'color_temp': 2856.0,
        'curves': [generate_spectral_curve(2856)]
    }
    light_sources.append(ls3)

    # 写入二进制文件
    with open(output_path, 'wb') as f:
        # 写入文件头
        f.write(struct.pack('<I', magic))
        f.write(struct.pack('<I', version))
        f.write(struct.pack('<I', num_sources))
        f.write(struct.pack('<I', mode_val))
        f.write(struct.pack('<I', complexity_val))

        # 写入每个光源
        for ls in light_sources:
            # 光源名称 (64字节固定长度)
            name_bytes = ls['name'].encode('utf-8')
            name_buf = bytearray(64)
            name_buf[:len(name_bytes)] = name_bytes
            f.write(name_buf)

            # 色温
            f.write(struct.pack('<d', ls['color_temp']))

            # 曲线数量
            num_curves = len(ls['curves'])
            f.write(struct.pack('<I', num_curves))

            # 写入每条曲线
            for curve in ls['curves']:
                num_bands = len(curve['wavelengths'])
                f.write(struct.pack('<I', num_bands))

                # 写入每个波段
                for wl, intensity in zip(curve['wavelengths'], curve['intensities']):
                    f.write(struct.pack('<d', wl))
                    f.write(struct.pack('<d', intensity))

    print(f"✓ Factory calibration binary file generated: {output_path}")

def generate_spectral_curve(color_temp):
    """
    生成光谱曲线数据
    """
    wavelengths = np.arange(380, 781, 10)  # 380-780nm，10nm间隔
    intensities = []

    for wl in wavelengths:
        # 简化的光谱响应模型，基于色温
        intensity = 0.0

        if wl < 440:
            intensity = 0.3 + 0.4 * (wl - 380) / 60.0
        elif wl < 490:
            intensity = 0.7 + 0.2 * (wl - 440) / 50.0
        elif wl < 510:
            intensity = 0.9 - 0.1 * (wl - 490) / 20.0
        elif wl < 580:
            intensity = 0.8 + 0.2 * (wl - 510) / 70.0
        elif wl < 645:
            intensity = 1.0 - 0.2 * (wl - 580) / 65.0
        else:
            intensity = 0.8 - 0.4 * (wl - 645) / 35.0

        # 根据色温调整
        if color_temp < 4000:
            # 暖光，增强红色
            intensity *= (1.0 + 0.3 * (1.0 - (wl - 380) / 400.0))
        elif color_temp > 6000:
            # 冷光，增强蓝色
            intensity *= (1.0 + 0.2 * (wl - 380) / 400.0)

        intensities.append(max(0.0, min(1.0, intensity)))

    return {
        'wavelengths': wavelengths.tolist(),
        'intensities': intensities
    }

if __name__ == '__main__':
    output_file = r'e:\Projects\Study\CameraToolKit\build\images\Spectral\factory_calib.dat'
    generate_factory_calib_bin(output_file)