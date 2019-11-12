import sys, re

# This takes on stdin a disassembly listing as for example generated
# by https://defuse.ca/online-x86-assembler.htm and turns it into an
# ugly but comprehensible C array literal

listing = sys.stdin.readlines()

opline = re.compile("^([0-9a-f]{1,3}):\\s+(([0-9a-f]{2} )+)\\s+(.*)$", re.A | re.I)
lbline = re.compile("^[0-9a-f]{16} <\\w+>:$", re.A | re.I)

lines = []

for line in listing:
    line = line.strip()
    if match := opline.match(line):
        addr = int(match.group(1), 16)
        ops = match.group(2)
        ops = re.sub("([0-9a-f]{2}) ", "0x\\1,", ops, flags=re.A|re.I)
        if ops == "0xcc," and lines[-1][0] == "ops" and re.match("^(0xcc,)+$", lines[-1][2]):
            lines[-1][2] += "0xcc,"
            if len(lines[-1][2]) == 8*5:
                lines[-1][3] = ".quad 0xcccccccccccccccc"
        else:
            lines.append(["ops", addr, ops, match.group(4)])
    elif match := lbline.match(line):
        lines.append(["addr", match.group(0)])
    else:
        print("Could not identify line \"{}\".".format(line))

longest = 0

for line in lines:
    if line[0] == "ops":
        if len(line[2]) > longest:
            longest = len(line[2])

for line in lines:
    if line[0] == "ops":
        length = longest - len(line[2])
        padding = " " * length
        print("/* {:03x}: */ {} {}// {}".format(line[1], line[2], padding, line[3]))
    elif line[0] == "addr":
        print("// {}".format(line[1]))
