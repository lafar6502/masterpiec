import io
import sys
import os
import datetime

if len(sys.argv) <= 2:
    print("no args. [input] [output]");
    exit()

print('input file', sys.argv[1])
print('out file', sys.argv[2]);

rrdpath = 'c:\\tools\\rrdtool\\rrdtool.exe'

def statenr(c):
    dic = {
        'P':0,
        '1':1,
        '2':2,
        'F':3,
        'A':4,
        'R':5,
        'r':6,
        'S':7,
        'z':8
    }
    return dic[c]



fn = os.path.basename(sys.argv[1])
mon = int(fn[1:3])
day = int(fn[3:5])

print('base name', fn, mon, day)

stm = io.open(file=sys.argv[1], mode='+rt');

ds = [
        ['TCO', 1], 
        ['TCWU', 2], 
        ['TRET', 3], 
        ['TEXH', 4], 
        ['TFEED', 5],  
        ['TBURN', 6], 
        ['DT13', 7],  
        ['STATE', 8, statenr], 
        ['NEEDH', 9], 
        ['BLOWER', 10], 
        ['PUMPCO', 11], 
        ['PUMPCW', 12], 
        ['PUMPCR', 13], 
        ['FEEDMS', 15], 
        ['BURNC', 22],
        ['DTEXH', 20],
        ['HEATER', 23],
        ['FLOWN', 24, lambda x:0 if int(x) == 207 else int(x)]
]

num = 0

stempl = ''
for dd in ds:
    stempl += ':' + dd[0]
stempl = stempl[1:]

while True:
    ln = stm.readline()
    num+=1
    if not ln:
        break
    if ln[0] == ';' or ln[0] == '#':
        continue

    dt = ln.split('\t')
    d0 = dt[0].split(':')
    hh = int(d0[0])
    mm = int(d0[1])
    dnow = datetime.date.today()
    y = dnow.year if mm < dnow.month or (mon == dnow.month and day <= dnow.day) else dnow.year - 1
    tstamp = datetime.datetime(year=y, month=mon, day=day, hour=hh, minute=mm)
    unixts = int(tstamp.timestamp())
    #print(num, tstamp, dt[0], unixts)

    s = str(unixts)
    for dd in ds:
        ix = dd[1]
        v = dt[ix]
        if len(dd) > 2 and dd[2]:
            v = dd[2](v)
        #s += ' ' + dd[0]
        s += ':'
        s += str(v)

    
    print(rrdpath, 'update ', sys.argv[2], '-t', stempl, s)

