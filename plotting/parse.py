import csv
colors = ['228654', '00A855', '228686', '00A8A8', '225486', '0055A8', '9A350E', 'A82E00', '9A620E', 'A86500', '538622', '27A800']
# for global graph
reader = csv.reader(open('results.csv', 'rb'), delimiter='\t')
garray = [];
gfiles = {};
for row in reader:
    if row[1] in garray:
        gfiles[int(row[1])].write("%s\t%s\n" % (row[0], row[2]))
    else:
        garray.append(row[1])
        gfiles[int(row[1])] = open('g-%s.out' % (row[1]), 'w')
        gfiles[int(row[1])].write("%s\t%s\n" % (row[0], row[2]))

i = iter(colors)
for num in garray:
    print "'g-%s.out' title '%s' lc rgb '#%s' lt 1 lw  1.5, \\" % (num, num, i.next())
# for local graph
reader = csv.reader(open('results.csv', 'rb'), delimiter='\t')
garray = [];
gfiles = {};
for row in reader:
    if row[0] in garray:
        gfiles[int(row[0])].write("%s\t%s\n" % (row[1], row[2]))
    else:
        garray.append(row[0])
        gfiles[int(row[0])] = open('l-%s.out' % (row[0]), 'w')
        gfiles[int(row[0])].write("%s\t%s\n" % (row[1], row[2]))

i = iter(colors)
for num in garray:
    print "'l-%s.out' title '%s' lc rgb '#%s' lt 1 lw  1.5, \\" % (num, num, i.next())
