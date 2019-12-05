# evaluate the results of the given patterns
from argparse import ArgumentParser
# import numpy as np
import os, datetime
from time import strftime, gmtime

def getArgs():
    parser = ArgumentParser()
    parser.add_argument('-inC', '--input_circuit', type=str, default=None)
    parser.add_argument('-inP', '--input_pattern', type=str, default=None)
    parser.add_argument('-inCD', '--input_circuit_dir', type=str, default=None)
    parser.add_argument('-inPD', '--input_pattern_dir', type=str, default=None)
    parser.add_argument('-ndet', '--n_detect', type=str, default='1')
    args = parser.parse_args()
    return args

def runTdfSim(ndet, inP, inC, tmpFile, verbose):
    command = './bin/golden_tdfsim -ndet {} -tdfsim {} {}'.format(ndet, inP, inC)
    if verbose: redirect = ' | tee {}'.format(tmpFile)
    else: redirect = ' > {}'.format(tmpFile)
    os.system(command + redirect)

def writeLog(ndet, inP, inC, tmpFile, logDir):
    if not os.path.isfile(tmpFile):
        print('Error: tmpFile not found.')

    nPat, nDetected, nTDF, t = 0, None, None, float('nan')
    with open(inP) as fp:
        for line in fp:
            #if line.startswith('#atpg: cputime for test pattern generation'):
            #    t = float(line.split(':')[2].split('s')[0])
            if line.startswith('#real used time:'):
                t = float(line.strip('\n').split(':')[1].strip('s').strip(' '))
                
    with open(tmpFile) as fp:
        for line in fp:
            if line.startswith('vector['): nPat += 1
            if line.startswith('# total transition delay faults:'):
                nTDF = int(line.split(':')[1])
            if line.startswith('# total detected faults:'):
                nDetected = int(line.split(':')[1])

    assert (nDetected!=None) and (nTDF!=None)
    os.remove(tmpFile)

    logFile = os.path.join(logDir, 'log.csv')
    if not os.path.isfile(logFile):
        with open(logFile, 'w') as fp:
            fp.write('timestamp,cktFile,patFile,ndet,#patterns,#detected,#TDF,FC(%),usedTime(s)\n')
    with open(logFile, 'a') as fp:
        timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        s = [timestamp, inC, inP, ndet, str(nPat), str(nDetected), str(nTDF), str(nDetected/nTDF*100), str(t)]
        fp.write(','.join(s) + '\n')


    logFile2 = os.path.join(logDir, inC.split('/')[-1].rstrip('.ckt'))
    os.makedirs(logFile2, exist_ok=True)
    logFile2 = os.path.join(logFile2, 'ndet{}.csv'.format(ndet))
    history = []
    if os.path.isfile(logFile2):
        with open(logFile2) as fp:
            fp.readline()
            for line in fp:
                line = line.strip('\n').split(',')
                history.append(line)
    rank, history = rerank(s, history)
    with open(logFile2, 'w') as fp:
        fp.write('timestamp,patFile,#patterns,#detected,#TDF,FC(%),usedTime(s)\n')
        for h in history:
            #h = [h[0]] + [h[2]] + h[4:]
            fp.write(','.join(h) + '\n')
    t = strftime("%M:%S", gmtime(t))
    return inC.split('/')[-1].rstrip('.ckt'), str(rank), s[7], s[4], t

def rerank(s, history):
    s = [s[0]] + [s[2]] + s[4:]
    r = len(history)
    for i,j in enumerate(history):
        if float(s[-1]) > 600.0: break
        fc1 = float(s[5])
        fc2 = float(j[5])
        npat1 = float(s[2])
        npat2 = float(j[2])

        cond1 = (fc1 >= fc2) and ((fc1-fc2)*npat1/10 > npat1-npat2)
        cond2 = (fc1 < fc2) and ((fc2-fc1)*npat2/10 < npat2-npat1)

        if cond1 or cond2:
            r = i + 1
            break

    return r, history[:r] + [s] + history[r:]


def getPair(inC, inP, inCD, inPD):
    ret = []
    if inC and inP: ret.append((inC, inP))
    Cs = os.listdir(inCD)
    Ps = os.listdir(inPD)
    for C in Cs:
        if C.rstrip('.ckt')+'.pat' in Ps:
            _c = os.path.join(inCD, C)
            _p = os.path.join(inPD, C.rstrip('.ckt')+'.pat')
            ret.append((_c, _p))
    return ret

def printLog(log):
    print()
    print('############### summary ################')
    print('     ckt    rank   FC(%)  length    time')
    for l in log:
        l = [i[:6].rjust(8, ' ') for i in l]
        print(''.join(l))
    print('################# end ##################')

if __name__ == '__main__':
    args = getArgs()
    ndet = args.n_detect
    inP = args.input_pattern
    inC = args.input_circuit
    inPD = args.input_pattern_dir
    inCD = args.input_circuit_dir

    # default settings
    tmpFile = 'testsuite/tmp.txt'
    logDir = 'testsuite/log/'
    os.makedirs(logDir, exist_ok=True)

    _log = []
    for _c,_p in getPair(inC, inP, inCD, inPD):
        print(_c, _p)
        runTdfSim(ndet, _p, _c, tmpFile, True)
        _log.append(writeLog(ndet, _p, _c, tmpFile, logDir))
    printLog(_log) 