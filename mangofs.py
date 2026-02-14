import io
import sys
import struct

class Date:
	def __init__(self, time):
		self.seconds = (time >> 40) & 0xFF
		self.minutes = (time >> 32) & 0xFF
		self.hours = (time >> 24) & 0xFF
		self.day = (time >> 16) & 0xFF
		self.month = (time >> 8) & 0xFF
		self.year = time & 0xFFFF

	def formatted(self):
		return f"{self.hours:02}:{self.minutes:02}:{self.seconds:02} {self.day:02}-{self.month:02}-{self.year}"

	def __repr__(self):
		return self.formatted()

class Node:
	def __init__(self):
		self.time_created = None
		self.time_changed = None
		self.parent = None
		self.child_head = None
		self.child_next = None
		self.size = None
		self.first_block = None
		self.name = None
		self.flags = None

	def get_type(self):
		name = "UNKNOWN"
		
		if self.flags & (1 << 0):
			name = "FILE"
		elif self.flags & (1 << 1):
			name = "FOLDER"

		return name

	def print(self):
		print("NAME:", self.name)
		print("CREATED:", self.time_created)
		print("CHANGED:", self.time_changed)
		print("PARENT:", self.parent)
		print("CHILD_HEAD:", self.child_head)
		print("CHILD_NEXT:", self.child_next)
		print("SIZE:", self.size)
		print("FIRST_BLOCK:", self.first_block)
		print("TYPE:", self.get_type())

class Block:
	def __init__(self):
		self.next = None
		self.data = None

class Disk:
	def __init__(self, file):
		self.disk = open(file, 'rb')

		self.sector(2048)
		self.sb_magic = self.disk.read(4).decode()

		if self.sb_magic != "MNGO":
			sys.exit("Disk has invalid magic number!")

		self.sb_version = struct.unpack("<I", self.disk.read(4))[0]
		self.sb_sectors = struct.unpack("<I", self.disk.read(4))[0]
		self.sb_used = struct.unpack("<I", self.disk.read(4))[0]
		self.sb_free = struct.unpack("<I", self.disk.read(4))[0]
		self.sb_free_list = struct.unpack("<I", self.disk.read(4))[0]
		self.root = self.read_node(2049)

	def print(self):
		print("MAGIC:", self.sb_magic)
		print("VERSION:", self.sb_version)
		print("SECTORS:", self.sb_sectors)
		print("USED:", self.sb_used)
		print("FREE:", self.sb_free)
		print("FREE_LIST:", self.sb_free_list)

	def sector(self, n):
		self.disk.seek(512 * n)

	def read_node(self, n):
		self.sector(n)

		node = Node()
		node.time_created = Date(struct.unpack("<Q", self.disk.read(8))[0])
		node.time_changed = Date(struct.unpack("<Q", self.disk.read(8))[0])
		node.parent = struct.unpack("<I", self.disk.read(4))[0]
		node.child_head = struct.unpack("<I", self.disk.read(4))[0]
		node.child_next = struct.unpack("<I", self.disk.read(4))[0]
		node.size = struct.unpack("<I", self.disk.read(4))[0]
		node.first_block = struct.unpack("<I", self.disk.read(4))[0]
		node.name = self.disk.read(32).decode("utf-8").rstrip("\x00")
		node.flags = struct.unpack("<B", self.disk.read(1))[0]

		return node

	def read_block(self, n):
		self.sector(n)

		block = Block()
		block.next = struct.unpack("<I", self.disk.read(4))[0]
		block.data = self.disk.read(508).decode("utf-8").rstrip("\x00")
		
		return block

	def read_data(self, node):
		if node.get_type() != "FILE":
			raise TypeError("Node is not readable!")

		data = io.StringIO()

		block = self.read_block(node.first_block)
		while block.next:
			data.write(block.data)
			block = self.read_block(block.next)
		data.write(block.data)

		return data

	def print_tree(self):
		current = self.root
		depth = 1
		print("- root/")

		def tree_from(parent, depth):
			child = parent
			while child:
				node = self.read_node(child)
				print(" " * depth, "-", node.name + ("/" if node.get_type() == "FOLDER" else ""))

				if node.get_type() == "FOLDER":
					tree_from(node.child_head, depth + 1)

				child = node.child_next

		tree_from(self.root.child_head, depth)

	def get_node(self, path):
		path = path.strip()

		if path[0] != "/":
			raise NameError("Path must start with '/'")

		current = self.root

		if path == "/":
			return current

		for name in path.split("/")[1:]:
			if name == "":
				continue

			found = False
			child = current.child_head

			while child:
				node = self.read_node(child)

				if node.name == name:
					found = True
					current = node
					break

				child = node.child_next

			if not found:
				return None

		return current

if __name__ == "__main__":
	args = sys.argv[1:]
	file = None

	if len(args) > 0:
		file = args[0]
	else:
		sys.exit("usage: mangofs <disk>")

	disk = Disk(file)

	if len(args) > 1:
		if args[1] == "tree":
			disk.print_tree()
		elif args[1] == "help":
			print("... tree\n\tprint the whole filesystem tree")
			print("... info <path>\n\tprint the full node info")
			print("... read <path>\n\tread a whole file")
		elif args[1] == "info":
			if len(args) < 3:
				sys.exit("usage: mangofs <disk> info <path>")

			if args[2][0] != '/':
				sys.exit("error: invalid path!")

			node = disk.get_node(args[2])
			if not node:
				sys.exit("error: not found!")

			node.print()
		elif args[1] == "read":
			if len(args) < 3:
				sys.exit("usage: mangofs <disk> read <path>")

			if args[2][0] != '/':
				sys.exit("error: invalid path!")

			node = disk.get_node(args[2])
			if not node:
				sys.exit("error: not found!")
			elif node.get_type() != "FILE":
				sys.exit("error: not readable!")

			data = disk.read_data(node)
			print(data.getvalue())
		else:
			print(f"error: unknown = {args[1]}")
	else:
		disk.print()