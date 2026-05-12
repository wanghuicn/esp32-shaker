import os
from PIL import Image, ImageDraw, ImageFont

def generate_ascii_font():
    # ASCII 可见字符范围 (32 空格 到 126 '~')
    CHARS = [chr(i) for i in range(32, 127)]
    HEIGHT = 40
    
    # 请确保系统中有这个字体，如果没有，请换成你电脑里有的英文字体，比如 arial.ttf
    try:
        font = ImageFont.truetype("arial.ttf", 36) 
    except IOError:
        try:
            font = ImageFont.truetype("msyh.ttc", 36)
        except IOError:
            print("找不到字体，请在同目录下放一个 arial.ttf")
            return

    with open('AsciiFont.h', 'w', encoding='utf-8') as f:
        f.write("#pragma once\n")
        f.write("#include <stdint.h>\n\n")
        f.write("// 高度固定为 40 像素的 ASCII 字体库\n")
        
        widths = []
        
        # 1. 遍历每个字符并生成数组
        for char in CHARS:
            # 获取字符尺寸
            bbox = font.getbbox(char)
            # 空格特殊处理
            char_w = 15 if char == ' ' else (bbox[2] - bbox[0] + 4) 
            widths.append(char_w)
            
            img = Image.new('1', (char_w, HEIGHT), color=0)
            draw = ImageDraw.Draw(img)
            
            # 居中绘制
            if char != ' ':
                draw.text((2, -2), char, font=font, fill=1)
            
            f.write(f"const uint8_t font_char_{ord(char)}[{char_w}][5] = {{\n")
            for col in range(char_w):
                f.write("  {")
                # 40个像素，压缩成 5个 uint8_t (5 * 8 = 40)
                col_data = [0, 0, 0, 0, 0]
                for row in range(HEIGHT):
                    if img.getpixel((col, row)):
                        col_data[row // 8] |= (1 << (row % 8))
                
                f.write(", ".join([f"0x{b:02X}" for b in col_data]))
                f.write("},\n" if col < char_w - 1 else "}\n")
            f.write("};\n\n")

        # 2. 生成指针索引表 (方便 C++ O(1) 查找)
        f.write("const uint8_t* const font_index[95] = {\n")
        for i, char in enumerate(CHARS):
            f.write(f"  (const uint8_t*)font_char_{ord(char)}")
            f.write("," if i < len(CHARS) - 1 else "")
            f.write(f" // '{char}'\n")
        f.write("};\n\n")
        
        # 3. 生成字符宽度表
        f.write("const uint8_t font_width[95] = {\n  ")
        f.write(", ".join([str(w) for w in widths]))
        f.write("\n};\n")
        
    print("✅ 成功生成 40px 高度字库 AsciiFont.h！请放入 src 目录。")

if __name__ == "__main__":
    generate_ascii_font()
