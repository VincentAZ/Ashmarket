"""
Takes anything that is not the red liquid out of a picture of red liquid that came with a see-through background.

    <a python with numpy and PIL> Tools/clean_blood_picture.py <picture.png> ...

A picture generated with nothing behind it sometimes keeps a faint grey cloud round the liquid where the studio's
shadow was. A pixel is kept for how red it is: grey has as much green and blue as red and goes, the liquid stays.
Nothing is drawn: every pixel left is the picture's own, and only how see-through it is changes. The picture as it
came is kept beside it as <name>_as_made.png the first time.
"""
import os
import sys

import numpy as np
from PIL import Image

for path in sys.argv[1:]:
    kept = path[:-4] + "_as_made.png"

    if not os.path.exists(kept):
        Image.open(path).save(kept)

    picture = np.asarray(Image.open(kept).convert("RGBA")).astype(np.float32) / 255.0
    red, green, blue, alpha = picture[..., 0], picture[..., 1], picture[..., 2], picture[..., 3]

    # how much redder than it is grey: nought for grey and white, near one for the liquid, dark or bright
    redness = (red - np.maximum(green, blue)) / np.maximum(red, 1e-3)
    keep = np.clip((redness - 0.25) / 0.3, 0.0, 1.0)

    before = float((alpha > 0.05).mean())
    picture[..., 3] = alpha * keep
    after = float((picture[..., 3] > 0.05).mean())

    Image.fromarray((picture * 255.0 + 0.5).astype(np.uint8), "RGBA").save(path)

    print("{}: {:.0f}% of the picture was something, {:.0f}% is now".format(os.path.basename(path), before * 100, after * 100))
