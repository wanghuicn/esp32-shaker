import os
from PIL import Image, ImageDraw, ImageFont

def generate_chinese_text():
    # 在这里输入你想滚动的长文本
    text ="清若荷花不染尘  廉如梅花不畏寒"     
    HEIGHT = 40
    # 字体大小设为 36，留一点上下边距
    FONT_SIZE = 40 
    
    # 请确保电脑里有这个中文字体文件
    try:
        font = ImageFont.truetype("得意黑.ttf", FONT_SIZE) 
    except IOError:
        try:
            font = ImageFont.truetype("simhei.ttf", FONT_SIZE)
        except IOError:
            print("❌ 找不到中文字体，请在目录下放一个 simhei.ttf")
            return

    # 1. 计算长文本的总宽度
    bbox = font.getbbox(text)
    WIDTH = bbox[2] - bbox[0] + 10 # 加 10 像素余量
    
    # 2. 生成黑白二值化图像
    img = Image.new('1', (WIDTH, HEIGHT), color=0)
    draw = ImageDraw.Draw(img)
    draw.text((5, -4), text, font=font, fill=1)
    
    # 3. 压缩并生成 C++ 数组
    with open('ChineseText.h', 'w', encoding='utf-8') as f:
        f.write("#pragma once\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"// 自动生成的中文长文本: {text}\n")
        f.write(f"const int CHINESE_TEXT_WIDTH = {WIDTH};\n")
        f.write(f"const uint8_t chinese_text_data[{WIDTH}][5] = {{\n")
        
        for col in range(WIDTH):
            f.write("  {")
            col_data = [0, 0, 0, 0, 0]
            for row in range(HEIGHT):
                # 如果该像素有文字，将对应比特位置 1
                if img.getpixel((col, row)):
                    col_data[row // 8] |= (1 << (row % 8))
            
            f.write(", ".join([f"0x{b:02X}" for b in col_data]))
            f.write("},\n" if col < WIDTH - 1 else "}\n")
            
        f.write("};\n")
        
    print(f"✅ 成功生成 ChineseText.h！宽度: {WIDTH} 列，请放入 src 目录。")

if __name__ == "__main__":
    generate_chinese_text()
