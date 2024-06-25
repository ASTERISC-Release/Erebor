file = open("stac_stats.txt", "r")

stac_map = {}
while(True):
    line = file.readline()
    if not line:
        break

    line = line.strip()
    if line not in stac_map:
        stac_map[line] = 1
    else:
        stac_map[line] += 1

print(stac_map)

file.close()
