# evaluate the results of the given patterns
from argparse import ArgumentParser
import os, datetime, time
from eval import getPair, runTdfSim, writeLog, printLog

def getArgs():
    parser = ArgumentParser()
    #parser.add_argument('-inC', '--input_circuit', type=str, default=None)
    parser.add_argument('-inCD', '--input_circuit_dir', type=str, default=None)
    parser.add_argument('-ndet', '--n_detect', type=str, default='1')
    parser.add_argument('-c', '--compress', action='store_true')
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('-bt', '--backtrack_limit', type=str, default='100')
    parser.add_argument('-rs', '--random_seed', type=str, default='160')
    args = parser.parse_args()
    return args

def runATPG(cktFile, patDir, ndet, cmpr, bt, rs, verbose):
    cmpr = '-compression' if cmpr else ''
    patFile = os.path.join(patDir, cktFile.split('/')[-1].rstrip('.ckt') + '.pat')
    command = './src/atpg -tdfatpg {} -ndet {} {}'.format(cmpr, ndet, cktFile)
    command += ' -bt {} -rs {}'.format(bt, rs)
    if verbose: redirect = ' | tee {}'.format(patFile)
    else: redirect = ' > {}'.format(patFile)
    t = time.time()
    os.system(command + redirect)
    t = time.time() - t
    with open(patFile, 'a') as fp:
        fp.write('#real used time: {}s\n'.format(str(t)))

def getAllCkt(inCD):
    #ret = [inC] if inC else []
    ret = []
    if inCD:
        for c in os.listdir(inCD):
            if c.endswith('.ckt'):
                ret.append(os.path.join(inCD, c))
    return ret

if __name__ == '__main__':
    args = getArgs()
    ndet = args.n_detect
    #inC = args.input_circuit
    inCD = args.input_circuit_dir
    cmpr = args.compress
    verbose = args.verbose
    bt = args.backtrack_limit
    rs = args.random_seed

    patDir = os.path.join('tdf_patterns/', datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S"))
    os.makedirs(patDir, exist_ok=True)
    for ckt in getAllCkt(inCD):
        runATPG(ckt, patDir, ndet, cmpr, bt, rs, verbose)

    tmpFile = 'testsuite/tmp.txt'
    logDir = 'testsuite/log/'
    os.makedirs(logDir, exist_ok=True)

    _log = []
    for _c,_p in getPair(None, None, inCD, patDir):
        runTdfSim(ndet, _p, _c, tmpFile, verbose)
        _log.append(writeLog(ndet, _p, _c, tmpFile, logDir))
    printLog(_log) 