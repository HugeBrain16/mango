include "struct.sc";
include "string.sc";

let cmd_isstruct = struct_isstruct;

let __cmd_name = 1;
let __cmd_args = 2;

func cmd_iscmd(cmd) {
    if (!cmd_isstruct(cmd, "cmd_cmd"))
        return false;

    let name = type_name(cmd[__cmd_name]);
    let args = type_name(cmd[__cmd_args]);

    return name == "str" && args == "list";
}

func cmd_parse(str, pfx) {
    str = string_trim(str);
    if (!string_startswith(str, pfx))
        return null;

    str = string_sub(str, sizeof(pfx));
    let slen = sizeof(str);

    let i = 0;
    let _name = true;
    let name = "";
    let arg = "";
    let args = list_init();
    while (i < slen) {
        if (_name && !string_iswhitespace(str[i]))
            name += str[i];
        else
            _name = false;

        if (!_name && str[i] == "\"") {
            i += 1;
            while (i < slen) {
                if (str[i] == "\"")
                    break;

                arg += str[i];
                i += 1;
            }

            list_push(args, arg);
            arg = "";
        } else if (!_name && !string_iswhitespace(str[i])) {
            arg += str[i];
        } else if (sizeof(arg) > 0) {
            list_push(args, arg);
            arg = "";
        }

        i += 1;
    }

    if (sizeof(arg) > 0)
        list_push(args, arg);

    let cmd = list_init();
    list_push(cmd, "cmd_cmd");
    list_push(cmd, name);
    list_push(cmd, args);

    return cmd;
}