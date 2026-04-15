import os
import io
import sys
import struct
from datetime import datetime

FILE = (1 << 0)
FOLDER = (1 << 1)

class Buffer(io.BytesIO):
	def __init__(self, size):
		super().__init__(b"\x00" * size)
		self.maxsize = size

	def write(self, data):
		if self.tell() + len(data) > self.maxsize:
			raise BufferError("overflow")
		return super().write(data)

def is_utf8(data):
	try:
		data.decode("utf-8")
		return True
	except UnicodeDecodeError:
		return False

def date_packed(time = None):
	if isinstance(time, datetime):
		date = time
	elif isinstance(time, Date):
		date = datetime.strptime(time.formatted(), "%H:%M:%S %d-%m-%Y")
	else:
		date = datetime.now()

	return date.second << 48 | \
		   date.minute << 40 | \
		   date.hour   << 32 | \
		   date.day    << 24 | \
		   date.month  << 16 | \
		   date.year

class Date:
	def __init__(self, time):
		self.seconds = (time >> 48) & 0xFF
		self.minutes = (time >> 40) & 0xFF
		self.hours 	 = (time >> 32) & 0xFF
		self.day 	 = (time >> 24) & 0xFF
		self.month 	 = (time >> 16) & 0xFF
		self.year 	 = (time) & 0xFFFF

	def formatted(self):
		return f"{self.hours:02}:{self.minutes:02}:{self.seconds:02} {self.day:02}-{self.month:02}-{self.year}"

	def __repr__(self):
		return self.formatted()

class Node:
	def __init__(self):
		self.sector = 0
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
		
		if self.flags & FILE:
			name = "FILE"
		elif self.flags & FOLDER:
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
		self.sector = 0
		self.next = 0
		self.data = b""

class Disk:
	def __init__(self, file):
		self.disk = open(file, 'r+b')
		self.formatted = False
		self.read_sb()

	def push(self, localfile, parent):
		filename = os.path.basename(localfile)

		file = self.get_node(f"{parent}/{filename}")
		if not file:
			self.node_create(parent, filename, FILE)
			file = self.get_node(f"{parent}/{filename}")

		self.file_write(file, open(localfile, "rb").read())

	def file_write(self, file, data):
		if isinstance(file, int):
			file = self.read_node(file)
		elif isinstance(file, str):
			file = self.get_node(file)

		file.size = 0
		written = 0
		length = len(data)

		block = self.read_block(file.first_block)
		while written < length:
			to_write = min(length - written, 508)
			block.data = data[:to_write]
			data = data[to_write:]
			written += to_write

			self.write_block(block)
			file.size += to_write
			self.write_node(file)

			if to_write == 508:
				if block.next == 0:
					node_sector = self.sector_alloc()
					if node_sector == 0:
						return False

					block.next = node_sector
					self.write_block(block)

					node = Block()
					node.sector = node_sector
					node.next = 0
					self.write_block(node)

					block = node
				else:
					block = self.read_block(block.next)

		if is_utf8(data):
			block.data += b"\x00";
			file.size += 1
			self.write_block(block)

		file.time_changed = date_packed()
		self.write_node(file)

		return True

	def node_create(self, parent, name, type):
		if isinstance(parent, int):
			parent = self.read_node(parent)
		elif isinstance(parent, str):
			parent = self.get_node(parent)

		node_sector = self.sector_alloc()
		if type == FILE:
			data_sector = self.sector_alloc()

		if node_sector == 0 or (type == FILE and data_sector == 0):
			if node_sector:
				self.sector_free(node_sector)
			if type == FILE and data_sector:
				self.sector_free(data_sector)

			return False

		node = Node()
		node.sector = node_sector
		node.time_created = date_packed()
		node.time_changed = date_packed()
		node.parent = parent.sector
		node.flags = type
		node.child_head = 0
		node.child_next = 0
		node.first_block = data_sector if type == FILE else 0
		node.size = 0
		node.name = name

		if parent.child_head:
			current = self.read_node(parent.child_head)
			while current.child_next:
				current = self.read_node(current.child_next)
			current.child_next = node_sector
			self.write_node(current)
		else:
			parent.child_head = node_sector
			self.write_node(parent)
		self.write_node(node)

		if type == FILE:
			data = Block()
			data.sector = data_sector
			data.next = 0
			self.write_block(data)

		return True

	def get_usable_sector_count(self):
		curr = self.disk.tell()

		self.disk.seek(0, 2)
		size = self.disk.tell()
		self.disk.seek(curr)

		return size // 512

	def read_sb(self):
		sector = self.disk.tell()

		self.sector(2048)
		self.sb_magic = self.disk.read(4).decode("utf-8")

		if self.sb_magic != "MNGO":
			self.formatted = False
			sys.stderr.write("warning: disk has invalid magic number, marked as unformatted.\n")
		else:
			self.formatted = True

		self.sb_version = struct.unpack("<I", self.disk.read(4))[0]
		self.sb_sectors = struct.unpack("<I", self.disk.read(4))[0]
		self.sb_used = struct.unpack("<I", self.disk.read(4))[0]
		self.sb_free = struct.unpack("<I", self.disk.read(4))[0]
		self.sb_free_list = struct.unpack("<I", self.disk.read(4))[0]
		self.root = self.read_node(2049)

		self.disk.seek(sector)

	def write_sb(self):
		sector = self.disk.tell()

		self.sector(2048)
		self.disk.write("MNGO".encode("utf-8"))
		self.disk.write(struct.pack("<I", self.sb_version))
		self.disk.write(struct.pack("<I", self.sb_sectors))
		self.disk.write(struct.pack("<I", self.sb_used))
		self.disk.write(struct.pack("<I", self.sb_free))
		self.disk.write(struct.pack("<I", self.sb_free_list))

		self.disk.seek(sector)

	def format(self):
		sector = self.disk.tell()

		self.sector(2048)
		self.disk.write("MNGO".encode("utf-8")) # magic
		self.disk.write(struct.pack("<I", 1)) # version
		self.disk.write(struct.pack("<I", self.get_usable_sector_count())) # sectors
		self.disk.write(struct.pack("<I", 2)) # used
		self.disk.write(struct.pack("<I", 2050)) # free
		self.disk.write(struct.pack("<I", 0)) # free_list

		self.disk.seek(sector)

		root = Node()
		root.sector = 2049
		root.time_created = date_packed()
		root.time_changed = date_packed()
		root.parent = 0
		root.flags = FOLDER
		root.child_head = 0
		root.child_next = 0
		root.size = 0
		root.first_block = 0
		root.name = "root"
		self.write_node(root)

		self.formatted = True

	def print(self):
		if self.formatted:
			print("MAGIC:", self.sb_magic)
			print("VERSION:", self.sb_version)
			print("SECTORS:", self.sb_sectors)
			print("USED:", self.sb_used)
			print("FREE:", self.sb_free)
			print("FREE_LIST:", self.sb_free_list)
		else:
			print("[ Unformatted ]")

	def sector(self, n):
		self.disk.seek(512 * n)

	def read_node(self, n):
		self.sector(n)

		node = Node()
		node.sector = n
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

	def write_node(self, node):
		sector = self.disk.tell()

		self.sector(node.sector)
		self.disk.write(struct.pack("<Q", date_packed(node.time_created)))
		self.disk.write(struct.pack("<Q", date_packed(node.time_changed)))
		self.disk.write(struct.pack("<I", node.parent))
		self.disk.write(struct.pack("<I", node.child_head))
		self.disk.write(struct.pack("<I", node.child_next))
		self.disk.write(struct.pack("<I", node.size))
		self.disk.write(struct.pack("<I", node.first_block))

		buffer = Buffer(32)
		buffer.write(node.name.encode("utf-8"))
		self.disk.write(buffer.getvalue())

		self.disk.write(struct.pack("<B", node.flags))

		self.disk.seek(sector)

	def read_block(self, n):
		self.sector(n)

		block = Block()
		block.sector = n
		block.next = struct.unpack("<I", self.disk.read(4))[0]
		block.data = self.disk.read(508)
		
		return block

	def write_block(self, block):
		sector = self.disk.tell()

		self.sector(block.sector)
		self.disk.write(struct.pack("<I", block.next))

		buffer = Buffer(508)
		buffer.write(block.data)
		self.disk.write(buffer.getvalue())

		self.disk.seek(sector)

	def read_data(self, node):
		if node.get_type() != "FILE":
			raise TypeError("Node is not readable!")

		data = io.BytesIO()

		block = self.read_block(node.first_block)
		while block.next:
			data.write(block.data)
			block = self.read_block(block.next)
		data.write(block.data)

		data = data.getvalue()[:node.size]
		return data

	def print_tree(self, head, list_only):
		current = head
		depth = 1

		if list_only:
			depth = 0
		else:
			print("- " + current.name + ("/" if current.get_type() == "FOLDER" else ""))

		def tree_from(parent, depth):
			child = parent
			while child:
				node = self.read_node(child)
				print(" " * depth, "-", node.name + ("/" if node.get_type() == "FOLDER" else ""))

				if node.get_type() == "FOLDER" and not list_only:
					tree_from(node.child_head, depth + 1)

				child = node.child_next

		tree_from(current.child_head, depth)

	def get_node(self, path):
		path = path.strip()

		if path[0] != "/":
			raise NameError("Path must start with '/'")

		current = self.read_node(self.root.sector)

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

	def sector_free(self, sector):
		self.read_sb()

		self.sector(sector)
		self.disk.write(struct.pack("<I", self.sb_free_list))

		self.sb_free_list = sector
		self.sb_used -= 1

		self.write_sb()

	def sector_alloc(self):
		self.read_sb()

		if self.sb_used > self.sb_sectors:
			sys.stderr.write("error: disk full!")
			return 0

		sector = 0
		if self.sb_free_list != 0:
			sector = self.sb_free_list

			curr = self.disk.tell()
			self.sector(sector)
			self.sb_free_list = struct.unpack("<I", self.disk.read(4))[0]
			self.disk.seek(curr)
		else:
			sector = self.sb_free
			self.sb_free += 1

		self.sb_used += 1
		self.write_sb()

		curr = self.disk.tell()
		self.sector(sector)
		self.disk.write(Buffer(512).getvalue())
		self.disk.seek(curr)

		return sector

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
			if not disk.formatted:
				sys.exit("error: disk is not formatted!")

			if len(args) >= 3:
				if args[2][0] != '/':
					sys.exit("error: invalid path!")

				node = disk.get_node(args[2])
				if not node:
					sys.exit("error: not found!")

				disk.print_tree(node, False)
			else:
				disk.print_tree(disk.root, False)
		elif args[1] == "list":
			if not disk.formatted:
				sys.exit("error: disk is not formatted!")

			if len(args) >= 3:
				if args[2][0] != '/':
					sys.exit("error: invalid path!")

				node = disk.get_node(args[2])
				if not node:
					sys.exit("error: not found!")

				disk.print_tree(node, True)
			else:
				disk.print_tree(disk.root, True)
		elif args[1] == "help":
			print("... tree [path]\n\tprint the whole filesystem tree")
			print("... list [path]\n\tlist items in a folder")
			print("... info <path>\n\tprint the full node info")
			print("... read <path>\n\tread a whole file")
			print("... push <localfile> <path>\n\tpush a local file into the disk")
		elif args[1] == "info":
			if not disk.formatted:
				sys.exit("error: disk is not formatted!")

			if len(args) < 3:
				sys.exit("usage: mangofs <disk> info <path>")

			if args[2][0] != '/':
				sys.exit("error: invalid path!")

			node = disk.get_node(args[2])
			if not node:
				sys.exit("error: not found!")

			node.print()
		elif args[1] == "read":
			if not disk.formatted:
				sys.exit("error: disk is not formatted!")

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

			try:
				print(data.decode("utf-8"))
			except UnicodeDecodeError:
				print(data)
		elif args[1] == "push":
			if not disk.formatted:
				sys.exit("error: disk is not formatted!")

			if len(args) < 4:
				sys.exit("usage: mangofs <disk> push <localfile> <path>")

			if not os.path.isfile(args[2]):
				sys.exit("error: not a file!")
			if args[3][0] != '/':
				sys.exit("error: invalid path!")

			node = disk.get_node(args[3])
			if not node:
				sys.exit("error: path not found!")
			elif node.get_type() != "FOLDER":
				sys.exit("error: path is not a folder!")

			disk.push(args[2], args[3])
		elif args[1] == "format":
			disk.format()
			print("Disk formatted successfuly!")
		else:
			print(f"error: unknown = {args[1]}")
	else:
		disk.print()