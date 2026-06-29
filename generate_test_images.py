import cv2
import numpy as np
import os

BASE_DIR = os.path.join(os.path.dirname(__file__), 'build', 'images')

def create_dir(path):
    os.makedirs(path, exist_ok=True)

def generate_dark_image(width=1280, height=720, noise_level=10):
    img = np.random.randint(0, noise_level, (height, width, 3), dtype=np.uint16)
    return img

def generate_bright_image(width=1280, height=720, base_intensity=20000, noise_level=50):
    img = np.full((height, width, 3), base_intensity, dtype=np.uint16)
    noise = np.random.randint(-noise_level, noise_level, (height, width, 3), dtype=np.int16)
    img = np.clip(img + noise, 0, 65535).astype(np.uint16)
    return img

def generate_flat_field_image(width=1280, height=720, intensity=30000):
    img = np.full((height, width, 3), intensity, dtype=np.uint16)
    gradient = np.linspace(0.9, 1.1, width)[np.newaxis, :, np.newaxis]
    img = (img * gradient).astype(np.uint16)
    return img

def generate_color_chart_image(width=640, height=480):
    # X-Rite ColorChecker标准24色卡颜色值 (sRGB, 8-bit)
    colors = [
        [115,  82,  68],   # Dark Skin
        [194, 150, 130],   # Light Skin
        [98,  122, 157],   # Blue Sky
        [87,  108,  67],   # Foliage
        [133, 128, 177],   # Blue Flower
        [103, 189, 170],   # Bluish Green
        [214, 126,  44],   # Orange
        [80,   91, 166],   # Purple
        [227, 188,  58],   # Yellow
        [157, 188,  64],   # Green
        [205,  75,  75],   # Red
        [133,  90, 156],   # Magenta
        [78,  157, 192],   # Cyan
        [245, 245, 245],   # White (90% Reflectance)
        [200, 200, 200],   # Neutral 8 (80% Reflectance)
        [160, 160, 160],   # Neutral 6.5 (65% Reflectance)
        [120, 120, 120],   # Neutral 5 (50% Reflectance)
        [80,   80,  80],   # Neutral 3.5 (35% Reflectance)
        [40,   40,  40],   # Neutral 2 (20% Reflectance)
        [10,   10,  10],   # Black (5% Reflectance)
        [255,   0,   0],   # Red (Pure)
        [0,   255,   0],   # Green (Pure)
        [0,     0, 255],   # Blue (Pure)
        [255, 255,   0]    # Yellow (Pure)
    ]
    
    img = np.zeros((height, width, 3), dtype=np.uint8)
    rows = 6
    cols = 4
    cell_w = width // cols
    cell_h = height // rows
    
    for i in range(rows):
        for j in range(cols):
            idx = i * cols + j
            color = colors[idx]
            img[i*cell_h:(i+1)*cell_h, j*cell_w:(j+1)*cell_w] = color
    
    return img

def generate_test_image(width=640, height=480, pattern='color'):
    if pattern == 'gradient':
        img = np.zeros((height, width, 3), dtype=np.uint8)
        for i in range(height):
            for j in range(width):
                img[i, j] = [j % 256, i % 256, (i+j) % 256]
    elif pattern == 'checkerboard':
        img = np.zeros((height, width, 3), dtype=np.uint8)
        square_size = 40
        for i in range(height // square_size):
            for j in range(width // square_size):
                if (i + j) % 2 == 0:
                    img[i*square_size:(i+1)*square_size, j*square_size:(j+1)*square_size] = 255
    elif pattern == 'gray':
        img = np.zeros((height, width), dtype=np.uint8)
        for i in range(height):
            img[i] = np.linspace(0, 255, width)
        img = cv2.cvtColor(img, cv2.COLOR_GRAY2BGR)
    elif pattern == 'color':
        img = generate_color_chart_image(width, height)
    elif pattern == 'noise':
        img = np.random.randint(0, 256, (height, width, 3), dtype=np.uint8)
    else:
        img = np.full((height, width, 3), 128, dtype=np.uint8)
    return img

def main():
    print("Generating test images...")
    
    create_dir(BASE_DIR)
    
    # SCM - Sensor Characterization Module (参考 verify_scm_fix.py 结构)
    # 结构: images/dark/exp_xxxus/dark_xxxus_0001.png
    #       images/bright/exp_xxxus/bright_xxxus_0001.png
    EXPOSURES = [1000.0, 2000.0, 4000.0]  # us
    NUM_FRAMES = 50
    
    dark_base_dir = os.path.join(BASE_DIR, 'dark')
    bright_base_dir = os.path.join(BASE_DIR, 'bright')
    create_dir(dark_base_dir)
    create_dir(bright_base_dir)
    
    for exp in EXPOSURES:
        exp_str = f"exp_{int(exp)}us"
        # Dark images
        dark_exp_dir = os.path.join(dark_base_dir, exp_str)
        create_dir(dark_exp_dir)
        # Bright images (不同曝光时间亮度不同)
        bright_exp_dir = os.path.join(bright_base_dir, exp_str)
        create_dir(bright_exp_dir)
        
        for i in range(NUM_FRAMES):
            # 暗帧：低噪声，随曝光时间略微增加
            dark_noise = int(10 + exp / 1000)
            dark = generate_dark_image(noise_level=dark_noise)
            dark_filename = f"dark_{int(exp)}us_{i+1:04d}.png"
            cv2.imwrite(os.path.join(dark_exp_dir, dark_filename), dark)
            
            # 亮帧：随曝光时间增加亮度，保持合理范围
            base_intensity = int(min(10000 + exp * 5, 50000))
            bright = generate_bright_image(base_intensity=base_intensity, noise_level=50)
            bright_filename = f"bright_{int(exp)}us_{i+1:04d}.png"
            cv2.imwrite(os.path.join(bright_exp_dir, bright_filename), bright)
    
    print(f"✓ SCM images generated ({len(EXPOSURES)} exposures × {NUM_FRAMES} frames each)")
    
    # Dead Pixel
    dead_pixel_dir = os.path.join(BASE_DIR, 'DeadPixel')
    create_dir(dead_pixel_dir)
    for i in range(3):
        dark = generate_dark_image(noise_level=20)
        dark[100, 200] = [65535, 65535, 65535]
        dark[150, 250] = [65535, 65535, 65535]
        dark[200, 300] = [0, 0, 0]
        cv2.imwrite(os.path.join(dead_pixel_dir, f'dark_{i+1:02d}.tiff'), dark)
    print("✓ Dead Pixel images generated")
    
    # Flat Field
    flat_field_dir = os.path.join(BASE_DIR, 'FlatField')
    create_dir(flat_field_dir)
    for i in range(3):
        flat = generate_flat_field_image()
        cv2.imwrite(os.path.join(flat_field_dir, f'flat_{i+1:02d}.tiff'), flat)
    print("✓ Flat Field images generated")
    
    # Quick Calib
    quick_calib_dir = os.path.join(BASE_DIR, 'QuickCalib')
    create_dir(quick_calib_dir)
    color_chart = generate_color_chart_image()
    cv2.imwrite(os.path.join(quick_calib_dir, 'color_chart.png'), color_chart)
    print("✓ Quick Calib images generated")
    
    # Color Error
    color_error_dir = os.path.join(BASE_DIR, 'ColorError')
    create_dir(color_error_dir)
    source = generate_color_chart_image()
    cv2.imwrite(os.path.join(color_error_dir, 'source.png'), source)
    ref = generate_color_chart_image()
    ref = cv2.addWeighted(ref, 0.95, np.zeros_like(ref), 0, 5)
    cv2.imwrite(os.path.join(color_error_dir, 'reference.png'), ref)
    print("✓ Color Error images generated")
    
    # Color Char
    color_char_dir = os.path.join(BASE_DIR, 'ColorChar')
    create_dir(color_char_dir)
    for i in range(3):
        chart = generate_color_chart_image()
        cv2.imwrite(os.path.join(color_char_dir, f'chart_{i+1:02d}.png'), chart)
    print("✓ Color Char images generated")
    
    # Spectral
    spectral_dir = os.path.join(BASE_DIR, 'Spectral')
    create_dir(spectral_dir)
    
    # Generate spectral curve CSV file
    csv_path = os.path.join(spectral_dir, 'spectral_curve.csv')
    with open(csv_path, 'w') as f:
        f.write('wavelength,R,G,B\n')
        for wl in range(400, 701, 10):
            r = max(0.1, 0.9 * np.exp(-((wl - 550) / 60)**2))
            g = max(0.1, 0.85 * np.exp(-((wl - 500) / 50)**2))
            b = max(0.1, 0.8 * np.exp(-((wl - 450) / 45)**2))
            f.write(f'{wl},{r:.4f},{g:.4f},{b:.4f}\n')
    print("✓ Spectral curve CSV generated")
    
    # Generate factory calibration DAT file
    dat_path = os.path.join(spectral_dir, 'factory_calib.dat')
    with open(dat_path, 'w') as f:
        f.write('[FactoryCalibration]\n')
        f.write('SensorModel=MV-CA050-10GM\n')
        f.write('CalibrationDate=2024-01-15\n')
        f.write('Temperature=25.0\n')
        f.write('Humidity=45.0\n')
        f.write('\n[SpectralSensitivity]\n')
        f.write('WavelengthRange=400-700\n')
        f.write('Resolution=10\n')
        for wl in range(400, 701, 10):
            r = max(0.1, 0.9 * np.exp(-((wl - 550) / 60)**2))
            g = max(0.1, 0.85 * np.exp(-((wl - 500) / 50)**2))
            b = max(0.1, 0.8 * np.exp(-((wl - 450) / 45)**2))
            f.write(f'{wl} {r:.6f} {g:.6f} {b:.6f}\n')
        f.write('\n[ColorFilters]\n')
        f.write('R_Center=620\n')
        f.write('R_FWHM=100\n')
        f.write('G_Center=530\n')
        f.write('G_FWHM=100\n')
        f.write('B_Center=460\n')
        f.write('B_FWHM=80\n')
        f.write('\n[CalibrationMatrix]\n')
        f.write('CCM_R=1.05, -0.12, 0.07\n')
        f.write('CCM_G=-0.08, 1.10, -0.02\n')
        f.write('CCM_B=0.03, -0.05, 1.02\n')
    print("✓ Factory calibration DAT generated")
    
    # Pipeline
    pipeline_dir = os.path.join(BASE_DIR, 'Pipeline')
    create_dir(pipeline_dir)
    test_img = generate_test_image(pattern='color')
    cv2.imwrite(os.path.join(pipeline_dir, 'input.png'), test_img)
    test_img2 = generate_test_image(pattern='gradient')
    cv2.imwrite(os.path.join(pipeline_dir, 'input_gradient.png'), test_img2)
    test_img3 = generate_test_image(pattern='checkerboard')
    cv2.imwrite(os.path.join(pipeline_dir, 'input_checkerboard.png'), test_img3)
    print("✓ Pipeline images generated")
    
    print("\nAll test images generated successfully!")
    print(f"Location: {BASE_DIR}")

if __name__ == '__main__':
    main()