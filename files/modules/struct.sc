let __struct_structname = 0;

func struct_isstruct(val, name) {
	if (type_name(val) != "list") return false;
	if (type_name(val[__struct_structname]) != "str") return false;

	return val[__struct_structname] == name;
}