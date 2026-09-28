"""Validate CPU PNG / actual GL-front-buffer PPM fixtures. Requires Pillow."""
import sys
from pathlib import Path
from PIL import Image
root = Path(sys.argv[1])
for scale in (1., 1.25, 1.5):
    images = [Image.open(root / f"{backend}-{scale:.6f}.{ext}").convert("RGB") for backend, ext in (("cpu", "png"), ("gpu", "ppm"))]
    samples=[]
    for im in images:
        def px(x,y): return im.getpixel((round(x*scale),round(y*scale)))
        assert px(42,41)==(255,255,255), "rounded corner must remain clear"
        assert px(190,85)[1]>245 and px(190,85)[0]<10 and px(190,85)[2]<10, "middle stop missing"
        assert px(190,225)[0]>240, "radial center is not red"
        assert px(394,80)==(255,255,255), "inset shadow leaks outside"
        a,b,c=(px(490,y)[0] for y in (44,53,80))
        assert a+20<b and b+10<c and c>245, f"inset has no blur falloff: {a,b,c}"
        assert px(393,255)[0]+10<px(380,255)[0], "outer shadow falloff missing"
        red=px(190,380)
        assert red[0]>250 and 123<=red[1]<=131 and 123<=red[2]<=131, "opacity layer is not half red"
        assert px(470,395)[0]>250 and px(510,395)[2]>250, "hard stop lost"
        samples.append([px(x,y) for x,y in ((190,85),(190,225),(490,44),(490,53),(490,80),(393,255),(190,380),(470,395),(510,395))])
    delta=max(abs(a-b) for left,right in zip(*samples) for a,b in zip(left,right))
    assert delta<=8, f"CPU / GPU probe delta {delta} exceeds tolerance"
    print(f"scale={scale}: 9 material probes per backend passed; max RGB delta={delta}")
