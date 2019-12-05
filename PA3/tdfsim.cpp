/**********************************************************************/
/*  Parallel-Fault Event-Driven Transition Delay Fault Simulator      */
/*                                                                    */
/**********************************************************************/

#include "atpg.h"

/* pack 16 faults into one packet.  simulate 16 faults togeter. 
 * the following variable name is somewhat misleading */
#define num_of_pattern 16


void ATPG::transition_delay_fault_simulation(int& total_detect_num) {

int i , j;
total_detect_num =0;
int current_detect_num =0;
fault_simulate_vectors(total_detect_num);
//compute_fault_coverage();

}
