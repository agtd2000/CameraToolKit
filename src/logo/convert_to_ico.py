from PIL import Image
import os

input_path = "logo.png"
output_path = "logo.ico"

if not os.path.exists(input_path):
    print(f"Error: {input_path} not found")
    exit(1)

img = Image.open(input_path)

sizes = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]

img.save(output_path, format='ICO', sizes=sizes)
print(f"Successfully converted to {output_path}")