"""
Graphics Programming 1 - Render Verification Tool
Compares a student render (screenshot) against a reference image.
No external dependencies required - uses only Python standard library.

Usage:
    python verify_render.py <your_screenshot.bmp> <reference.bmp> [--viewer]

Example:
    python verify_render.py screenshots/my_screenshot.bmp references/week01_step0_normals.bmp
    python verify_render.py screenshots/my_screenshot.bmp references/week01_step1_depth.bmp --viewer
"""

import struct
import sys
import os
import tkinter as tk

# --- BMP Reading / Writing (standard library only) ---

def read_bmp(filepath):
    """Read a BMP file and return (width, height, pixels).
    Pixels is a list of (r, g, b) tuples in top-to-bottom, left-to-right order.
    Supports 24-bit and 32-bit uncompressed BMPs.
    """
    with open(filepath, "rb") as f:
        # File header (14 bytes)
        sig = f.read(2)
        if sig != b"BM":
            raise ValueError(f"Not a BMP file: {filepath}")
        _file_size, _, _, pixel_offset = struct.unpack("<I2HI", f.read(12))

        # DIB header (at least 40 bytes for BITMAPINFOHEADER)
        dib_size = struct.unpack("<I", f.read(4))[0]
        if dib_size < 40:
            raise ValueError(f"Unsupported BMP DIB header size: {dib_size}")

        width, height = struct.unpack("<iI", f.read(8))
        _planes, bpp = struct.unpack("<HH", f.read(4))
        compression = struct.unpack("<I", f.read(4))[0]

        # compression 0 = BI_RGB (uncompressed), 3 = BI_BITFIELDS (channel masks)
        if compression not in (0, 3):
            raise ValueError(f"Compressed BMPs are not supported (compression={compression})")
        if bpp not in (24, 32):
            raise ValueError(f"Only 24-bit and 32-bit BMPs are supported (got {bpp}-bit)")

        # Read color masks for BI_BITFIELDS
        r_mask, g_mask, b_mask = 0x00FF0000, 0x0000FF00, 0x000000FF
        if compression == 3:
            # Skip remaining BITMAPINFOHEADER fields (image_size, xppm, yppm,
            # colors_used, colors_important) to reach color masks
            f.read(20)
            r_mask, g_mask, b_mask = struct.unpack("<III", f.read(12))

        # Precompute shift amounts from masks
        def mask_shift(mask):
            if mask == 0:
                return 0, 0
            shift = 0
            while (mask >> shift) & 1 == 0:
                shift += 1
            bits = 0
            m = mask >> shift
            while m & 1:
                bits += 1
                m >>= 1
            return shift, bits

        r_shift, r_bits = mask_shift(r_mask)
        g_shift, g_bits = mask_shift(g_mask)
        b_shift, b_bits = mask_shift(b_mask)

        # Skip rest of DIB header
        f.seek(pixel_offset)

        # BMP rows are bottom-to-top and padded to 4-byte boundaries
        bytes_per_pixel = bpp // 8
        row_size = width * bytes_per_pixel
        row_padding = (4 - (row_size % 4)) % 4

        rows = []
        for _ in range(height):
            row_data = f.read(row_size + row_padding)
            row = []
            for x in range(width):
                off = x * bytes_per_pixel
                if bpp == 32:
                    pixel = struct.unpack_from("<I", row_data, off)[0]
                    r = ((pixel & r_mask) >> r_shift) * 255 // ((1 << r_bits) - 1)
                    g = ((pixel & g_mask) >> g_shift) * 255 // ((1 << g_bits) - 1)
                    b = ((pixel & b_mask) >> b_shift) * 255 // ((1 << b_bits) - 1)
                else:
                    b = row_data[off]
                    g = row_data[off + 1]
                    r = row_data[off + 2]
                row.append((r, g, b))
            rows.append(row)

        # Flip to top-to-bottom
        rows.reverse()

        pixels = []
        for row in rows:
            pixels.extend(row)

        return width, height, pixels


def write_bmp(filepath, width, height, pixels):
    """Write a 24-bit BMP file. Pixels is a list of (r, g, b) tuples,
    top-to-bottom, left-to-right order.
    """
    row_size = width * 3
    row_padding = (4 - (row_size % 4)) % 4
    pixel_data_size = (row_size + row_padding) * height
    file_size = 14 + 40 + pixel_data_size

    with open(filepath, "wb") as f:
        # File header
        f.write(b"BM")
        f.write(struct.pack("<I2HI", file_size, 0, 0, 54))

        # DIB header (BITMAPINFOHEADER)
        f.write(struct.pack("<IiiHHIIiiII",
            40, width, height, 1, 24, 0,
            pixel_data_size, 2835, 2835, 0, 0))

        # Pixel data (bottom-to-top)
        padding_bytes = b"\x00" * row_padding
        for y in range(height - 1, -1, -1):
            row_start = y * width
            for x in range(width):
                r, g, b = pixels[row_start + x]
                f.write(struct.pack("BBB", b, g, r))
            f.write(padding_bytes)


# --- Comparison ---

def compare_images(student_pixels, ref_pixels, width, height, tolerance=2):
    """Compare two pixel buffers. Returns (match_count, mismatch_count, diff_pixels).
    diff_pixels is a list of (r,g,b) for a visual diff image.
    """
    total = width * height
    match_count = 0
    diff_pixels = []

    for i in range(total):
        sr, sg, sb = student_pixels[i]
        rr, rg, rb = ref_pixels[i]

        if (abs(sr - rr) <= tolerance
                and abs(sg - rg) <= tolerance
                and abs(sb - rb) <= tolerance):
            match_count += 1
            diff_pixels.append((0, 0, 0))
        else:
            # Bright red for mismatched pixels
            diff_pixels.append((255, 0, 0))

    mismatch_count = total - match_count
    return match_count, mismatch_count, diff_pixels


# --- Viewer ---

def pixels_to_photoimage(pixels, width, height):
    """Convert a pixel list to a tkinter PhotoImage using single bulk put."""
    img = tk.PhotoImage(width=width, height=height)
    # Build all rows in one string: "{#rrggbb #rrggbb ...} {#rrggbb ...} ..."
    rows = []
    for y in range(height):
        s = y * width
        rows.append("{" + " ".join(
            f"#{pixels[s+x][0]:02x}{pixels[s+x][1]:02x}{pixels[s+x][2]:02x}"
            for x in range(width)) + "}")
    img.put(" ".join(rows))
    return img


def open_viewer(student_pixels, ref_pixels, diff_pixels, width, height,
                match_pct, student_path, ref_path):
    """Open a tkinter window with a wipe slider, diff checkbox, and zoom."""
    root = tk.Tk()
    root.title(f"Render Verification - {match_pct:.1f}% match")
    root.resizable(False, False)

    # Pre-build per-pixel hex strings in a 2D grid [y][x] for incremental
    # column updates. Also build full image strings for endpoints/diff.
    print("Building viewer (this may take a moment)...")
    student_hex = []
    ref_hex = []
    for y in range(height):
        s = y * width
        student_hex.append([
            f"#{student_pixels[s+x][0]:02x}{student_pixels[s+x][1]:02x}{student_pixels[s+x][2]:02x}"
            for x in range(width)])
        ref_hex.append([
            f"#{ref_pixels[s+x][0]:02x}{ref_pixels[s+x][1]:02x}{ref_pixels[s+x][2]:02x}"
            for x in range(width)])

    # Flat byte arrays for fast PPM-based full-image updates
    student_bytes = bytes(c for pixel in student_pixels for c in pixel)
    ref_bytes = bytes(c for pixel in ref_pixels for c in pixel)
    diff_bytes = bytes(c for pixel in diff_pixels for c in pixel)
    row_bytes = width * 3
    ppm_header = f"P6\n{width} {height}\n255\n".encode()

    import tempfile
    ppm_tmp_path = os.path.join(tempfile.gettempdir(), "_verify_render.ppm")

    def put_ppm(img, raw_bytes):
        """Update a PhotoImage in-place using PPM via temp file (C-level I/O)."""
        with open(ppm_tmp_path, "wb") as f:
            f.write(ppm_header)
            f.write(raw_bytes)
        tmp = tk.PhotoImage(file=ppm_tmp_path)
        img.tk.call(img, "copy", tmp)

    def build_composite_bytes(x):
        """Build wipe composite as bytes via fast memcpy slicing."""
        result = bytearray(len(student_bytes))
        xb = x * 3
        for y in range(height):
            off = y * row_bytes
            result[off:off + xb] = student_bytes[off:off + xb]
            result[off + xb:off + row_bytes] = ref_bytes[off + xb:off + row_bytes]
        return bytes(result)

    zoom = [1]

    # Scrollable canvas
    viewer_frame = tk.Frame(root)
    viewer_frame.pack(fill=tk.BOTH, expand=True)

    canvas = tk.Canvas(viewer_frame, width=width, height=height,
                       highlightthickness=0)
    h_scroll = tk.Scrollbar(viewer_frame, orient=tk.HORIZONTAL,
                            command=canvas.xview)
    v_scroll = tk.Scrollbar(viewer_frame, orient=tk.VERTICAL,
                            command=canvas.yview)
    canvas.config(xscrollcommand=h_scroll.set, yscrollcommand=v_scroll.set)
    canvas.grid(row=0, column=0, sticky="nsew")
    v_scroll.grid(row=0, column=1, sticky="ns")
    h_scroll.grid(row=1, column=0, sticky="ew")
    viewer_frame.grid_rowconfigure(0, weight=1)
    viewer_frame.grid_columnconfigure(0, weight=1)

    # Source image at original resolution
    source_img = tk.PhotoImage(width=width, height=height)
    put_ppm(source_img, student_bytes)

    zoomed_img = [source_img]  # currently displayed (may be zoomed copy)
    canvas_item = canvas.create_image(0, 0, anchor=tk.NW, image=source_img)
    canvas.config(scrollregion=(0, 0, width, height))

    # Vertical split line (yellow, on top of the image)
    split_line = canvas.create_line(width, 0, width, height,
                                    fill="yellow", width=2)

    last_x = [width]
    sel_pixel = [None]  # (ix, iy) of selected pixel, or None

    # Selection highlight rectangle (initially hidden)
    sel_rect = canvas.create_rectangle(0, 0, 1, 1, outline="yellow",
                                       width=2, state=tk.HIDDEN)

    def apply_zoom():
        """Zoom source_img and update canvas display."""
        z = zoom[0]
        if z == 1:
            zoomed_img[0] = source_img
        else:
            zoomed_img[0] = source_img.zoom(z, z)
        canvas.itemconfig(canvas_item, image=zoomed_img[0])
        canvas.config(scrollregion=(0, 0, width * z, height * z))
        if not show_diff.get():
            lx = last_x[0] * z
            canvas.coords(split_line, lx, 0, lx, height * z)
        if sel_pixel[0] is not None:
            sx, sy = sel_pixel[0]
            canvas.coords(sel_rect, sx * z, sy * z,
                          (sx + 1) * z, (sy + 1) * z)

    def full_rebuild():
        """Full rebuild of source image from current state, then apply zoom."""
        x = last_x[0]
        if show_diff.get():
            put_ppm(source_img, diff_bytes)
        elif x <= 0:
            put_ppm(source_img, ref_bytes)
        elif x >= width:
            put_ppm(source_img, student_bytes)
        else:
            put_ppm(source_img, build_composite_bytes(x))
        apply_zoom()

    def on_slider_change(val):
        if show_diff.get():
            return
        x = int(float(val))
        if x == last_x[0]:
            return
        old_x = last_x[0]
        last_x[0] = x
        z = zoom[0]

        # Update split line
        canvas.coords(split_line, x * z, 0, x * z, height * z)

        # Update source image (always at original resolution)
        if 0 <= old_x <= width:
            # Incremental: only repaint the columns that changed
            lo = min(old_x, x)
            hi = max(old_x, x)
            source = student_hex if x > old_x else ref_hex
            strip_rows = []
            for y in range(height):
                strip_rows.append(
                    "{" + " ".join(source[y][lo:hi]) + "}")
            source_img.put(" ".join(strip_rows), to=(lo, 0))
        else:
            # Coming from unknown state (e.g. diff toggle) — full rebuild
            if x <= 0:
                put_ppm(source_img, ref_bytes)
            elif x >= width:
                put_ppm(source_img, student_bytes)
            else:
                put_ppm(source_img, build_composite_bytes(x))

        # Re-zoom if needed (C-level pixel replication)
        if z > 1:
            apply_zoom()

    # Scroll wheel to zoom, centered on cursor
    def on_mousewheel(event):
        old_z = zoom[0]
        if event.delta > 0:
            zoom[0] = min(old_z + 1, 8)
        else:
            zoom[0] = max(old_z - 1, 1)
        if zoom[0] == old_z:
            return "break"

        # Remember image-space point under cursor
        cx = canvas.canvasx(event.x)
        cy = canvas.canvasy(event.y)
        ix = cx / old_z
        iy = cy / old_z

        full_rebuild()
        zoom_label.config(text=f"{zoom[0]}x")

        # Scroll so the same image point stays under the cursor
        new_z = zoom[0]
        sr_w = width * new_z
        sr_h = height * new_z
        if sr_w > 0:
            canvas.xview_moveto((ix * new_z - event.x) / sr_w)
        if sr_h > 0:
            canvas.yview_moveto((iy * new_z - event.y) / sr_h)
        return "break"

    root.bind("<MouseWheel>", on_mousewheel)

    # Left-click drag to pan
    canvas.bind("<ButtonPress-1>",
                lambda e: canvas.scan_mark(e.x, e.y))
    canvas.bind("<B1-Motion>",
                lambda e: canvas.scan_dragto(e.x, e.y, gain=1))

    # Controls - labels row
    label_row = tk.Frame(root)
    label_row.pack(fill=tk.X)
    tk.Label(label_row, text=os.path.basename(student_path)).pack(side=tk.LEFT)
    tk.Label(label_row, text=os.path.basename(ref_path)).pack(side=tk.RIGHT)

    # Slider aligned to canvas width
    slider = tk.Scale(root, from_=0, to=width, orient=tk.HORIZONTAL,
                      showvalue=False, length=width)
    slider.set(width)
    slider.pack()
    slider.config(command=on_slider_change)

    # Bottom controls
    bottom = tk.Frame(root)
    bottom.pack(fill=tk.X, padx=8, pady=(0, 4))

    show_diff = tk.BooleanVar(value=False)
    tk.Checkbutton(bottom, text="Show diff",
                   variable=show_diff).pack(side=tk.LEFT)

    # Pixel info (right-click or type coordinates)
    pixel_info = tk.Frame(bottom)
    pixel_info.pack(side=tk.LEFT, padx=(16, 0))
    tk.Label(pixel_info, text=f"{width}x{height}").pack(side=tk.LEFT,
                                                         padx=(0, 8))
    pos_entry = tk.Entry(pixel_info, width=10)
    pos_entry.pack(side=tk.LEFT)
    tk.Button(pixel_info, text="\u00d7", width=2, pady=0,
              command=lambda: clear_selection()).pack(side=tk.LEFT)
    tk.Label(pixel_info, text=" Left:").pack(side=tk.LEFT)
    yours_swatch = tk.Frame(pixel_info, width=14, height=14,
                            relief=tk.SUNKEN, bd=1)
    yours_swatch.pack(side=tk.LEFT, padx=(2, 2))
    yours_swatch.pack_propagate(False)
    yours_val = tk.Label(pixel_info, text="", anchor=tk.W)
    yours_val.pack(side=tk.LEFT)
    tk.Label(pixel_info, text=" Right:").pack(side=tk.LEFT)
    ref_swatch = tk.Frame(pixel_info, width=14, height=14,
                          relief=tk.SUNKEN, bd=1)
    ref_swatch.pack(side=tk.LEFT, padx=(2, 2))
    ref_swatch.pack_propagate(False)
    ref_val = tk.Label(pixel_info, text="", anchor=tk.W)
    ref_val.pack(side=tk.LEFT)

    def show_pixel(ix, iy):
        """Update pixel info display for given image coordinates."""
        ix = max(0, min(ix, width - 1))
        iy = max(0, min(iy, height - 1))
        idx = iy * width + ix
        sr, sg, sb = student_pixels[idx]
        rr, rg, rb = ref_pixels[idx]
        pos_entry.delete(0, tk.END)
        pos_entry.insert(0, f"{ix}, {iy}")
        yours_swatch.config(bg=f"#{sr:02x}{sg:02x}{sb:02x}")
        yours_val.config(text=f"({sr}, {sg}, {sb})")
        ref_swatch.config(bg=f"#{rr:02x}{rg:02x}{rb:02x}")
        ref_val.config(text=f"({rr}, {rg}, {rb})")
        # Show selection rectangle
        sel_pixel[0] = (ix, iy)
        z = zoom[0]
        canvas.coords(sel_rect, ix * z, iy * z,
                      (ix + 1) * z, (iy + 1) * z)
        canvas.itemconfig(sel_rect, state=tk.NORMAL)

    def clear_selection():
        sel_pixel[0] = None
        canvas.itemconfig(sel_rect, state=tk.HIDDEN)
        pos_entry.delete(0, tk.END)
        yours_swatch.config(bg=root.cget("bg"))
        yours_val.config(text="")
        ref_swatch.config(bg=root.cget("bg"))
        ref_val.config(text="")

    def on_pos_entry(_event=None):
        text = pos_entry.get().strip().strip("()")
        parts = [p.strip() for p in text.replace(",", " ").split()]
        if len(parts) == 2:
            try:
                show_pixel(int(parts[0]), int(parts[1]))
            except ValueError:
                pass

    pos_entry.bind("<Return>", on_pos_entry)

    def on_right_click(event):
        z = zoom[0]
        ix = int(canvas.canvasx(event.x) / z)
        iy = int(canvas.canvasy(event.y) / z)
        show_pixel(ix, iy)

    canvas.bind("<ButtonPress-3>", on_right_click)

    def reset_zoom():
        zoom[0] = 1
        zoom_label.config(text="1x")
        full_rebuild()

    tk.Button(bottom, text="Reset 100%",
              command=reset_zoom).pack(side=tk.RIGHT)
    zoom_label = tk.Label(bottom, text="1x", width=3, anchor=tk.E)
    zoom_label.pack(side=tk.RIGHT, padx=(0, 4))
    tk.Label(bottom, text="Zoom:").pack(side=tk.RIGHT)

    def on_diff_toggle(*_args):
        if show_diff.get():
            slider.config(state=tk.DISABLED)
            canvas.itemconfig(split_line, state=tk.HIDDEN)
            full_rebuild()
        else:
            slider.config(state=tk.NORMAL)
            canvas.itemconfig(split_line, state=tk.NORMAL)
            last_x[0] = -1  # force refresh
            on_slider_change(slider.get())

    show_diff.trace_add("write", on_diff_toggle)

    print("Viewer ready. Scroll wheel to zoom, drag to pan.")
    root.update_idletasks()
    root.geometry(root.geometry())
    root.mainloop()


# --- Main ---

def main():
    # Parse --viewer flag from any position
    args = [a for a in sys.argv[1:] if a != "--viewer"]
    use_viewer = "--viewer" in sys.argv

    if len(args) != 2:
        print("Usage: python verify_render.py <your_screenshot.bmp> <reference.bmp> [--viewer]")
        sys.exit(1)

    path_a = args[0]
    path_b = args[1]

    # Validate files are .bmp and exist
    for p in (path_a, path_b):
        if not p.lower().endswith(".bmp"):
            print(f"ERROR: Only .bmp files are supported: {p}")
            sys.exit(1)
        if not os.path.isfile(p):
            print(f"ERROR: File not found: {p}")
            sys.exit(1)

    # Detect which is the reference (lives in a "references" folder) -
    # order of arguments does not matter.
    a_is_ref = "references" in os.path.normpath(path_a).split(os.sep)
    b_is_ref = "references" in os.path.normpath(path_b).split(os.sep)

    if a_is_ref and not b_is_ref:
        ref_path, student_path = path_a, path_b
    elif b_is_ref and not a_is_ref:
        ref_path, student_path = path_b, path_a
    else:
        if a_is_ref and b_is_ref:
            print("WARNING: Both files are inside a 'references' folder.")
        else:
            print("WARNING: Neither file is inside a 'references' folder.")
        print(f"  Treating first argument as screenshot, second as reference.\n")
        student_path, ref_path = path_a, path_b

    # Read images
    print(f"Reading student screenshot: {student_path}")
    sw, sh, student_pixels = read_bmp(student_path)

    print(f"Reading reference: {ref_path}")
    rw, rh, ref_pixels = read_bmp(ref_path)

    # Resolution check
    if sw != rw or sh != rh:
        print(f"\nERROR: Resolution mismatch!")
        print(f"  Your screenshot:  {sw} x {sh}")
        print(f"  Reference:        {rw} x {rh}")
        print(f"\nMake sure your window size matches the reference resolution.")
        sys.exit(1)

    # Compare
    print(f"Comparing {sw}x{sh} images (tolerance: +/-2 per channel)...")
    match_count, mismatch_count, diff_pixels = compare_images(
        student_pixels, ref_pixels, sw, sh)

    total = sw * sh
    match_pct = (match_count / total) * 100.0

    # Results
    print()
    if mismatch_count == 0:
        print(f"PASS - All {total} pixels match!")
    else:
        print(f"FAIL - {mismatch_count}/{total} pixels differ ({match_pct:.1f}% correct)")

        # Write diff image
        diff_path = os.path.splitext(student_path)[0] + "_diff.bmp"
        write_bmp(diff_path, sw, sh, diff_pixels)
        print(f"\nDiff image saved to: {diff_path}")
        print("  Red pixels = where your output differs from the reference")
        print("  Dark pixels = where your output matches")

    # Open interactive viewer if requested
    if use_viewer:
        open_viewer(student_pixels, ref_pixels, diff_pixels, sw, sh, match_pct,
                    student_path, ref_path)


if __name__ == "__main__":
    main()
