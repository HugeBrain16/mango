let password = input("Password: ");
while (password != "meow") {
	exec("clear");
	println("Wrong password!");
	password = input("Password: ");
}

exec("clear");
println("Meow!");