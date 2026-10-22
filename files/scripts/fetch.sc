include "string.sc";
include "unit.sc";

func cfg(name, default) {
    if (!config_has("/system/config/fetch.cfg", name))
        return default;

    return config_get("/system/config/fetch.cfg", name);
}

func bool(val) {
    if (type_name(val) == "bool") return val;
    return string_bool(val);
}

#- config -#
let kernel = bool(cfg("kernel", true));
let cpu    = bool(cfg("cpu",    true));
let memory = bool(cfg("memory", true));
let uptime = bool(cfg("uptime", true));

let disk      = bool(cfg("disk",     true));
let disk_name = bool(cfg("disk_name", true));

let color_label = cfg("color_label", null);
let color_value = cfg("color_value", null);

# --- #

func print_label(name, space) {
    if (color_label)
        color_setfg(color_label);
    print(name + ":");
    if (space)
        print(" ");
    if (color_label)
        color_reset();
}

func print_value(value, newline) {
    if (color_value)
        color_setfg(color_value);
    print(value);
    if (newline)
        println();
    if (color_value)
        color_reset();
}

let args = argv();
for (let i = 0; i < argc(); i += 1) {
    let arg = args[i];

    if (arg == "diskname") disk_name = true;
    else if (arg == "nokernel") kernel = false;
    else if (arg == "nocpu") cpu = false;
    else if (arg == "nomemory") memory = false;
    else if (arg == "nouptime") uptime = false;
}

# == Display ==

#- kernel -#
if (kernel) {
    print_label("Kernel", true);
    print_value("Mango b" + as_str(os_buildnumber()), true);
}

#- CPU -#
if (cpu) {
    print_label("CPU", true);
    print_value(cpu_name(), true);
}

#- Memory -#
if (memory) {
    print_label("Memory", true);
    print_value(
        unit_size_str(mem_total()) +
        " (Free: " + unit_size_str(mem_free()) + ")", true);
}

#- Disk -#
if (disk) {
    print_label("Disk", true);
    if (disk_name)
        print_value(ata_model() + " - ", false);
    print_value(unit_size_str(ata_size()) + " (Used: " + unit_size_str(ata_used()) + ")", true);
}

#- Uptime -#
if (uptime) {
    print_label("Uptime", false);
    if (os_uptime_hours() > 0)
        print_value(" " + as_str(os_uptime_hours()) + " hours", false);
    if (os_uptime_minutes() > 0)
        print_value(" " + as_str(os_uptime_minutes()) + " minutes", false);
    if (os_uptime_seconds() > 0)
        print_value(" " + as_str(os_uptime_seconds()) + " seconds", false);
    println();
}