import os
from PIL import Image, ImageDraw

os.makedirs("D:/Data/ui", exist_ok=True)

img = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
d = ImageDraw.Draw(img)

# Two circles for the top bumps of the heart
d.ellipse([2, 4, 32, 34], fill=(220, 30, 30, 255))
d.ellipse([28, 4, 58, 34], fill=(220, 30, 30, 255))

# Triangle for the bottom point
d.polygon([(2, 22), (30, 62), (58, 22)], fill=(220, 30, 30, 255))

img.save("D:/Data/ui/heart_icon.png")
print("heart_icon.png written to D:/Data/ui/")
