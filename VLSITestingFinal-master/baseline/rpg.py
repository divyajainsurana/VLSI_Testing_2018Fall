# random pattern generator
from argparse import ArgumentParser
import numpy as np
import os

def getArgs():
    parser = ArgumentParser()
    parser.add_argument('-inF', '--input_file', type=str, default=None)
    parser.add_argument('-inD', '--input_directory', type=str, default=None)
    parser.add_argument('-outD', '--output_directory', type=str, default='.')
    parser.add_argument('-k', '--k_patterns', type=int, default=128)
    parser.add_argument('-nr', '--non_repetitive', action='store_true')
    args = parser.parse_args()
    return args
    
def readCktIn(fn):
    cnt = 1
    with open(fn) as fp:
        for line in fp:
            if line[0] == 'i':
                cnt +=1
    return fn.split('/')[-1].rstrip('.ckt'), cnt
    
def getFileLst(inF, inD):
    ret = []
    if inF: ret.append(inF)
    if inD:
        fns = filter(lambda k: k.endswith('.ckt'), os.listdir(inD))
        fns = [os.path.join(inD, i) for i in fns]
        ret += fns
    return ret

def prepro(inF, inD):
    fns = getFileLst(inF, inD)
    return [readCktIn(fn) for fn in fns]
    
def genPats(l, k, nr):
    ret = []
    if nr: k = min(k, 2**l)
    while len(ret) < k:
        while len(ret) < k:
            ret.append(np.random.randint(2, size=l))
        if nr:  ret = list(set(ret))
    return ret
    
def genAllPats(inCkts, outD, k, nr):
    os.makedirs(outD, exist_ok=True)
    for i,j in inCkts:
        pats = genPats(j, k, nr)
        outF = os.path.join(outD, i+'.r{}.pat'.format(str(len(pats))))
        with open(outF, 'w') as fp:
            for pat in pats:
                s = ''.join([str(p) for p in pat])
                s = s[:-1] + ' ' + s[-1]
                fp.write('T\'{}\'\n'.format(s))
    
if __name__ == '__main__':
    args = getArgs()
    inCkts = prepro(args.input_file, args.input_directory)
    genAllPats(inCkts, args.output_directory,
               args.k_patterns, args.non_repetitive)
    