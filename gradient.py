from entrypoint2 import entrypoint

# last color channel
chan = {
    8: 0x07,
    16: 0x1F,
    24: 0xFF,
    32: 0xFF,
}


@entrypoint
def gradient(width=320, height=240, colorbit=32, fileout="/dev/fb0"):
    """colorbit: 8/16/24/32"""
    assert colorbit in chan
    with open(fileout, "wb") as f:
        for y in range(0, height):
            for x in range(0, width):
                c = int(chan[colorbit] * (x + y) / (width + height))
                f.write((c).to_bytes(int(colorbit / 8), byteorder="little"))
