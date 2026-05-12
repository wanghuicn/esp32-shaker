import tkinter as tk
from tkinter import filedialog
from PIL import Image, ImageEnhance
import os

def convert_images():
    root = tk.Tk()
    root.withdraw() 
    
    # 允许选择多个文件
    file_paths = filedialog.askopenfilenames(
        title="选择多张要转换的图片 (按住Ctrl多选)",
        filetypes=[("Image Files", "*.png *.jpg *.jpeg *.bmp")]
    )
    
    if not file_paths:
        print("未选择文件，程序退出。")
        return

    WIDTH = 100
    HEIGHT = 40
    NUM_IMAGES = len(file_paths)
    
    gamma = 2.2
    gamma_table = [int(((i / 255.0) ** gamma) * 255 + 0.5) for i in range(256)]

    try:
        with open('ImageGallery.h', 'w', encoding='utf-8') as f:
            f.write("#pragma once\n")
            f.write("#include <stdint.h>\n\n")
            f.write(f"// 自动生成的多图画廊，包含 {NUM_IMAGES} 张图片\n")
            f.write(f"#define NUM_IMAGES {NUM_IMAGES}\n\n")
            
            # 定义三维数组: [第几张图][列X][行Y]
            f.write(f"const uint32_t image_gallery[NUM_IMAGES][{WIDTH}][{HEIGHT}] = {{\n")
            
            for idx, path in enumerate(file_paths):
                print(f"正在处理 [{idx+1}/{NUM_IMAGES}]: {os.path.basename(path)}")
                
                img_original = Image.open(path).convert("RGBA")
                bg = Image.new("RGBA", img_original.size, (0, 0, 0, 255))
                img_composite = Image.alpha_composite(bg, img_original).convert("RGB")
                
                enhancer = ImageEnhance.Contrast(img_composite)
                img_enhanced = enhancer.enhance(1.5)
                img = img_enhanced.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)
                
                f.write("  {\n") # 单张图片的开始
                for col in range(WIDTH):
                    f.write("    {")
                    for row in range(HEIGHT):
                        r, g, b = img.getpixel((col, row))
                        
                        if r < 30 and g < 30 and b < 30:
                            r, g, b = 0, 0, 0
                        else:
                            r = gamma_table[r]
                            g = gamma_table[g]
                            b = gamma_table[b]
                            
                        hex_color = f"0x{r:02X}{g:02X}{b:02X}"
                        f.write(f"{hex_color}")
                        if row < HEIGHT - 1:
                            f.write(", ")
                    f.write("}")
                    if col < WIDTH - 1:
                        f.write(",\n")
                    else:
                        f.write("\n")
                f.write("  }") # 单张图片的结束
                if idx < NUM_IMAGES - 1:
                    f.write(",\n\n")
                else:
                    f.write("\n")
                    
            f.write("};\n")
            
        print(f"\n✅ 成功！已生成 ImageGallery.h，共包含 {NUM_IMAGES} 张图片。")
        print("请将该文件放入 PlatformIO 的 src 目录中。")
        
    except Exception as e:
        print(f"❌ 处理失败: {e}")

if __name__ == "__main__":
    convert_images()