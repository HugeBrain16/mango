let version = "1.0";
let argc = argc();
let prefix = "/system/scripts/";

func format(name) {
  return prefix + name + ".sc";
}

func header() {
  println("SM (script manager) v" + version);
  println();
}

func help() {
  header();
  println("List subcommands:");
  println("help - shows this");
  println("edit - open editor for a script");
  println("new  - creates a new script");
  println("rm   - deletes an existing script");
}

func edit() {
  if (argc > 1) {
    let path = format(argv(1));
    if (file_isfile(path))
      exec("edit " + path);
    else
      println("Not found.");
  } else
    exec("edit " + format("sm"));
}

func rm() {
  if (argc > 1) {
    let path = format(argv(1));
    if (file_isfile(path))
      exec("delfile " + path);
    else
      println("Not found.");
  } else
    println("Usage: sm rm <name>");
}

func new() {
  if (argc > 1) {
    let path = format(argv(1));
    if (file_isfile(path) == false)
      exec("newfile " + path);
    else
      println("Already exist.");
  } else
    println("Usage: sm new <name>");
}

if (argc > 0) {
  let cmd = argv(0);

  if (cmd == "help") help();
  else if (cmd == "edit") edit();
  else if (cmd == "new") new();
  else if (cmd == "rm") rm();
  else println("Unknown command: " + cmd);
} else {
  header();
  println("No subcommand provided.");
  println("try 'sm help'");
}
