/**********************************************************************/
/*           This is the levelling routine for atpg                   */
/*                                                                    */
/*           Author: Bing-Chen (Benson) Wu                            */
/*           last update : 01/21/2018                                 */
/**********************************************************************/

#include <utility>
#include "atpg.h"

void ATPG::level_circuit(void) {
  nptr ncurrent,nprelast;
  bool schedule;
  int level = 0;
  list<nptr> event_list;         // event list linked queue
  forward_list<wptr> wire_stack; // stack of wires to be labeled for a given level
  
  /* levels are propagated from PI to PO (like events) */
  /* build up the initial node event list */
  for (wptr wptr_ele: cktin) {
    sort_wlist.push_back(wptr_ele);
    wptr_ele->wlist_index = (int)sort_wlist.size()-1;
    wptr_ele->flag |= MARKED; // MARKED means the PI is already leveled
    wptr_ele->level = level;  // PI level =0
    for (nptr nptr_ele: wptr_ele->onode) {
      if (find(event_list.begin(), event_list.end(), nptr_ele) == event_list.end()) // if this node not yet in event list
        event_list.push_front(nptr_ele); //add to the front of event list
    }
  }
  nprelast = *event_list.rbegin();
  /*  the event list linked queue is like this
	n             nnnnnnnnnnnnnnnnnnnnnnnnnnnnnNNNNNNNNNNNNNNNNNNNNNNNNNNNNN
	^             ^                           ^                            ^
	ncurrent      *event_list.begin()         nprelast                     *event_list.rbegin()

	ncurrent points to the node under evaluation now
	event_list.begin() points to the front of the queue  (exit)
	event_list.end() points to the end of queue (entrance)
	n = nodes to be evaluated for this level
  N = nodes to be evaluated for the next level
  */
  while (!event_list.empty()) { // go through all nodes in the event list (for this level)
    ncurrent = event_list.front(); // get a node from the front of event list
    event_list.pop_front();
    schedule = true;
    for (wptr wptr_ele: ncurrent->iwire) { // check every gate input
      if (!(wptr_ele->flag & MARKED)) { // if any one gate input is not marked yet
        schedule = false;  // do not schedule this event
      }
    }
    if (schedule) {
      for (wptr wptr_ele: ncurrent->owire) {
        sort_wlist.push_back(wptr_ele);  //insert node output wire to sort_wlist
        wptr_ele->wlist_index = (int)sort_wlist.size()-1;
        wire_stack.push_front(wptr_ele); // push node output to stack to be leveled
        for (nptr nptr_ele: wptr_ele->onode) { // propagate the event to fanout node
          if (find(event_list.begin(), event_list.end(), nptr_ele) == event_list.end()) // if not in the event list
            event_list.push_back(nptr_ele); // append to end of event list
        }
      }
    } // if schedule
    else { // if this node is not scheduled
      if (find(event_list.begin(), event_list.end(), ncurrent) == event_list.end()) // if not in the event list
        event_list.push_back(ncurrent); // re-push this node back to the event list linked queue
    }
    
    /* when finish all nodes in the event list for a given level,
	     mark level to all wires in the stack*/ 
    if (ncurrent == nprelast) {
      level++;
      while (!wire_stack.empty()) { // pop every wire in the stack and mark them
        wire_stack.front()->flag |= MARKED;
        wire_stack.front()->level = level;
        wire_stack.pop_front();
      }
      nprelast = *event_list.rbegin();
    } // if ncurent
  } // while (nfirst) , starts the next level

  /* after the whole circuit is done, unmark each wire */
  for (wptr wptr_ele: sort_wlist) {
    wptr_ele->flag &= ~MARKED;
    //cout << wptr_ele->name << " " << wptr_ele->level << endl;
  }
  //cout << "size of sort_wlist: " << sort_wlist.size() << endl;
}/* end of level */

/* for all gates in the circuits,
   put smaller level gate input before the larger level gate input 
   so that we can speed up the evaluation of a gate */
void ATPG::rearrange_gate_inputs(void) {
  nptr n;
  for (size_t i = cktin.size(); i < sort_wlist.size(); i++) {
    if ( (n = sort_wlist[i]->inode.front())) { // check every gate in the circuit
      for (size_t j = 0; j < n->iwire.size(); j++) {  // check every pait of gate inputs
        for (size_t k = j + 1; k < n->iwire.size(); k++) {
          if (n->iwire[j]->level > n->iwire[k]->level) { // if order is wrong
            swap(n->iwire[j], n->iwire[k]); // swap the gate inptuts
          }
        }
      }
    }
  }
  
  //nptr n;
  //for (int i = cktin.size(); i < sort_wlist.size(); i++) {
  //  if (n = sort_wlist[i]->inode.front()) { // check every gate in the circuit
  //    stable_sort(n->iwire.begin(), n->iwire.end(), // sort the gate inputs by the order of their level
  //                [](const wptr w1, const wptr w2){return w1->level < w2->level;}); // using lambda as sort criterion
  //  }
  //}
}/* end of rearrange_gate_inputs */
