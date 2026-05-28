#!/usr/bin/env python3
"""GIF 写入 helper — 由 C++ 编辑器通过 stdin JSON 调用。

输入 (stdin JSON):
{"output":"/path/to/out.gif","frames":"/tmp/gifframes/","count":99,"durations":[100,...],"loop":0}

frames 目录下为 frame_0000.png, frame_0001.png ...
"""

import sys, json, os
from PIL import Image

def main():
    data = json.load(sys.stdin)
    out = data["output"]
    frame_dir = data["frames"]
    count = int(data["count"])
    durations = data.get("durations", [100] * count)
    loop = data.get("loop", 0)

    imgs = []
    for i in range(count):
        fn = os.path.join(frame_dir, f"frame_{i:04d}.png")
        if os.path.exists(fn):
            imgs.append(Image.open(fn))
        else:
            break

    if not imgs:
        print("ERROR: no frames found")
        sys.exit(1)

    # Ensure all frames are same size and RGB
    w, h = imgs[0].size
    for i, img in enumerate(imgs):
        if img.size != (w, h):
            img = img.resize((w, h))
        if img.mode != "RGB" and img.mode != "RGBA":
            img = img.convert("RGB")
        imgs[i] = img

    dur = durations[0] if durations else 100
    imgs[0].save(out, save_all=True, append_images=imgs[1:],
                 duration=durations, loop=loop, optimize=True)
    size = os.path.getsize(out)
    print(f"OK {size}")

if __name__ == "__main__":
    main()
