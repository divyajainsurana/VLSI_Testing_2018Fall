# VLSI Testing Final Project
  This repository is for NTU course "VLSI Tesing" final project.
  The subject of the project is described in "**doc/testing final project.pdf**"

## compile
   type "make" under project directory or src/.  

## testsuite usage
   eval single pattern: **python3 testsuite/eval.py -inC <input_ckt> -inP <input_pat> [-ndet <n_det>]**  
   eval multiple patterns: **python3 testsuite/eval.py -inCD <input_ckt_dir> -inPD <input_pat_dir> [-ndet <n_det>]**  
   test atpg: **python3 testsuite/test.py -inCD <input_ckt_dir> [-ndet <n_det>] [-c (compression)]**  
   python3 is recommanded, no external package required. Logs are stored under testsuit/log.
   
## python interface for STC
   method: QM + ILP  
   requirements: pulp, tqdm (can be installed by pip).  
   type **"python3 baseline/stc.py -h"** for usage.  


## slides and report
   slides: https://docs.google.com/presentation/d/1uHWJgAasKoXtPl82FDMgLfbHBRGDt38AaUD7YPMYSGA/edit?usp=sharing  
   report: https://docs.google.com/document/d/1WnJLYVUEM8OOqz4yu_7ELE0PofY3frdPpTB0KfQAkSM/edit?usp=sharing  
