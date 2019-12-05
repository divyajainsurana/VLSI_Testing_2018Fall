# generate a complete fault dictionary of the given circuit
import os
from tqdm import tqdm
from time import time
    
class table2D():
    def __init__(self, faultDict):
        # each item in faultDict: (pat, [detected faults])
        fSet = set()
        for p, fL in faultDict:
            fSet |= set(fL)
        
        self.fLst = sorted(list(fSet), key=lambda k: int(k))
        self.fIdMap, self._fIdMap = dict(), dict()
        for i,f in enumerate(self.fLst):
            self.fIdMap[f] = i
            self._fIdMap[i] = f
        
        self.pIdMap, self._pIdMap = dict(), dict()
        self.pLst = []
        self.fpTbl = [[0 for i in range(len(faultDict))] for j in range(len(self.fLst))]
        for i,j in enumerate(faultDict):
            p, fL = j
            self.pLst.append(p)
            self.pIdMap[p] = i
            self._pIdMap[i] = p
            for f in fL:
                self.fpTbl[self.fIdMap[f]][i] = 1
        
    def dumpCsv(fn):
        pass

    
def tdfSim(pat, cktFile):
    # simulate a pattern
    t = str(time())
    patFn = t + '.pat'
    logFn = t + '.log'
    # run tdfsim
    with open(patFn, 'w') as fp:
        fp.write('T\'{}\'\n'.format(pat))
    command = './bin/golden_tdfsim_faults -ndet 1 -tdfsim {} {} > {}'.format(patFn, cktFile, logFn)
    os.system(command)
    # get the faults detected
    ret  = []
    with open(logFn) as fp:
        for line in fp:
            if line.startswith('fault'):
                line = line.rstrip('\n').lstrip('fault ')
                Fid, _ = line.split(': ')
                #print('Fid: ',Fid)
                ret.append(Fid)
            if line.startswith('vector[0] detects'):
                #assert len(ret) == int(line.split('(')[1].split(')')[0])
                break
    os.remove(patFn)
    os.remove(logFn)
    return ret

def simAllPat(cktFile):
    # simulate all patterns and return a complete fault dictionary
    nIn = 0
    with open(cktFile) as fp:
        for line in fp:
            if line[0] == 'i':
                nIn +=1
    y = []
    print('simulating...')
    for i in tqdm(range(2**(nIn+1))):
        pat = bin(i)[2:].zfill(nIn+1)
        pat = pat[:-1] + ' ' + pat[-1]
        #print(pat)
        x = tdfSim(pat, cktFile)
        if len(x) > 0:
            y.append((pat, x))
    return table2D(y)
    
def simPatFromFile(patFile, cktFile):
    print(cktFile)
    pats = []
    with open(patFile) as fp:
        for line in fp:
            if line.startswith('T'):
                pats.append(line.lstrip('T\'').rstrip('\'\n'))
    y = []
    for pat in tqdm(pats):
        x = tdfSim(pat, cktFile)
        if len(x) > 0:
            y.append((pat, x))
    return table2D(y)
        
if __name__ == '__main__':
    cktFile = 'sample_circuits/c17.ckt'
    x = simAllPat(cktFile)