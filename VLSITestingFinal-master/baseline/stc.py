# perform static fault compression using QM(ILP)
# if input pattern not specified, exhaustive measure would be used

from pulp import *
from argparse import ArgumentParser
from genFaultDict import simAllPat, simPatFromFile
from time import time

def getArgs():
    parser = ArgumentParser()
    parser.add_argument('-inC', '--input_circuit', type=str, default=None)
    parser.add_argument('-inP', '--input_pattern', type=str, default=None)
    parser.add_argument('-o', '--output_pattern', type=str, default='test.pat')
    parser.add_argument('-ndet', '--n_detect', type=int, default=1)
    args = parser.parse_args()
    return args

""" LpSatus
{ 0: 'Not Solved',
  1: 'Optimal'   ,
 -1: 'Infeasible',
 -2: 'Unbounded' ,
 -3: 'Undefined'  }
"""

def solveLpProb(fDict, ndet, patFn):
    # build ILP prob
    prob = LpProblem('STC', LpMinimize)
    lvLst = [LpVariable(p ,cat='Binary') for p in fDict.pLst]
    for i,row in enumerate(fDict.fpTbl):
        if sum(row) >= ndet:
            constraint = 0
            for j,e in enumerate(row):
                constraint += lvLst[j] * e
            prob += constraint >= ndet
            
    prob += sum(lvLst)
    status = prob.solve()
    assert status == 1
    
    # write results
    with open(patFn, 'w') as fp:
        for i,lv in enumerate(lvLst):
            #print(value(lv))
            if value(lv) == 1:
                p = fDict._pIdMap[i]
                fp.write('T\'{}\'\n'.format(p))
                
if __name__ == '__main__':
    args = getArgs()
    cktFile = args.input_circuit
    ndet = args.n_detect
    iPatFn = args.input_pattern
    oPatFn = args.output_pattern
    if iPatFn: fd = simPatFromFile(iPatFn, cktFile)
    else: fd = simAllPat(cktFile)
    t = time()
    solveLpProb(fd, ndet, oPatFn)
    print('solving ILP took', time()-t, 's.')