let __unit_gb = 1073741824;
let __unit_mb = 1048576;
let __unit_kb = 1024;

func unit_gb(size) {
    return as_int(size / __unit_gb);
}

func unit_gb_frac(size) {
    return as_int(((size % __unit_gb) * 100) / __unit_gb);
}

func unit_mb(size) {
    return as_int(size / __unit_mb);
}

func unit_mb_frac(size) {
    return as_int(((size % __unit_mb) * 100) / __unit_mb);
}

func unit_kb(size) {
    return as_int(size / __unit_kb);
}

func unit_kb_frac(size) {
    return as_int(((size % __unit_kb) * 100) / __unit_kb);
}

func unit_size_str(size) {
    let gb = unit_gb(size);
    let mb = unit_mb(size);
    let kb = unit_kb(size);
    let frac;

    if (gb > 0) {
        frac = unit_gb_frac(size);
        return as_str(gb) + "." + as_str(frac) + " GB";
    } else if (mb > 0) {
        frac = unit_mb_frac(size);
        return as_str(mb) + "." + as_str(frac) + " MB";
    } else if (kb > 0) {
        frac = unit_kb_frac(size);
        return as_str(kb) + "." + as_str(frac) + " KB";
    }

    return as_str(size) + " B";
}