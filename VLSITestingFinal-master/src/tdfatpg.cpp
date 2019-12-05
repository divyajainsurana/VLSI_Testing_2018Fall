#include "atpg.h"

#include <cassert>
#include <algorithm>
#include <climits>
#include <bitset>

int inverted_fault_type( int fault_type );

/*
 *  Note:
 *
 *    Currently, do not support
 *
 *      1. bracktrack in v2 when there is no decision tree ( set uniquely implied value )
 *
 *    All these problem needs to modify podem algorithm.
 *
 *    The generated patterns are stored in global variable **vectors** ( in class ATPG ),
 *    and these patterns contain don't cares.
 */
void ATPG::tdf_atpg()
{
  // set flags
  bool run_simple_stc = true;
  bool run_qm_ilp     = true;
  bool run_ts         = false;
  
  // set min pattern size for QM to solve
  const size_t minVecSize = max(800/(9-detected_num), 0);
  
  fptr fault_under_test;
  
  tdfReRank(flist_undetect, set<size_t>(), INT_MAX);

  printf("#compress = %d, detect_num = %d\n", perform_STC||perform_DTC, detected_num);

  while( ( fault_under_test = select_a_fault() ) )
  {
#if !defined( NDEBUG ) && false
    display_fault( fault_under_test );
#endif

    tdf_generate_pattern( fault_under_test );
    fault_under_test->test_tried = true;
  }
  tdf_duplicate_pattern();
  
  /* preform STC */
  if( perform_STC )
  {
    run_simple_stc = run_simple_stc && (vectors.size() > minVecSize);
    if( run_simple_stc  ) simpleSTC();
    if( run_ts          ) tdf_ts_compression();
    if( run_qm_ilp      ) solveQMILP(!run_simple_stc, minVecSize);
  }

  for( const string &pattern : vectors )
     printf( "T'%s %c'\n", pattern.substr( 0, cktin.size() ).c_str(), pattern.back() );
}

void ATPG::tdf_ts_compression()
{
	
	vector<string> &patterns=vectors;
	
	const int length=patterns.size();
	int size=cktin.size()+1;
	int length_1=9999;
	int i, j, k;
	
	//
	/*patterns.push_back("02210");
	patterns.push_back("02212");
	patterns.push_back("02012");
	patterns.push_back("01220");
	patterns.push_back("20220");
	patterns.push_back("12222");
	patterns.push_back("21200");
	patterns.push_back("11110");
	*/
	/*for(i=0;i<patterns.size();i++){
		cout<<patterns[i]<<endl;
	}*/
	bitset<10000> b;
	b.reset();
	vector<bitset<10000> > matrix(length,b);
	vector<int> appear;
	int same_num;
	for(i=0;i<length;i++){
		for(j=i+1;j<length;j++){
			same_num=0;
			for(k=0;k<size;k++){
				if(patterns[i][k]=='2' || patterns[j][k]=='2'){
					same_num++;
					continue;
				}
				else if(patterns[i][k]!=patterns[j][k]) break;
				else{
					same_num++;
					continue;
				}

			}
			//cout<<i<<" "<<j<<" "<<same_num<<endl;
			if(same_num==size){
					matrix[i].flip(length_1-j);
					matrix[j].flip(length_1-i); 
			}
			
		}
	}
	
	
/*	for(i=0;i<length;i++){
		cout<<matrix[i]<<endl;
	}
	cout<<endl;
	*/
	
	int vec[2];
	int tmp_count, max_count=-1;
	int flag=1;
	while(flag==1){
		vec[0]=-1;vec[1]=-1;
		max_count=-1;
		flag=0;
		for(i=0;i<length;i++){
			for(j=i+1;j<length;j++){
				if(matrix[i].test(length_1-j)==1&&matrix[j].test(length_1-i)==1){
					tmp_count=(matrix[i]&matrix[j]).count();
					if(tmp_count>max_count){
						max_count=tmp_count;
						vec[0]=i;vec[1]=j;
						flag=1;
						
					}
				}
				
				
			}
		}
		if(flag==1){
			matrix[vec[0]]&=matrix[vec[1]];
			matrix[vec[1]].reset();
			//cout<<"(vi, vj)="<<vec[0]<<", "<<vec[1]<<endl;
			appear.push_back(vec[1]); 
			for(k=0;k<size;k++){
				if(patterns[vec[0]][k]=='2' && patterns[vec[1]][k]=='2') continue;
				else if(patterns[vec[0]][k]=='2') patterns[vec[0]][k]=patterns[vec[1]][k];
			}
			/*for(i=0;i<length;i++){
				cout<<matrix[i]<<endl;
			}*/
		}
		
	} 
	
	sort(appear.begin(), appear.end());
	for (std::vector<int>::iterator it = appear.end()-1 ; it != appear.begin()-1; --it){
		patterns.erase(patterns.begin()+(*it));
	}
	
	
	/*for(i=0;i<patterns.size();i++){
		cout<<patterns[i]<<endl;
	}*/
	
}


ATPG::fptr ATPG::select_a_fault()
{
  for( const fptr fault : flist_undetect )
     if( !fault->test_tried )
       return fault;
  return nullptr;
}

/*!
 *  Select a fault for dynamic compression.
 */
ATPG::fptr ATPG::select_a_fault( const fptr primary_fault, const forward_list<fptr> &flist  )
{
  return ( flist.empty() ) ? nullptr : flist.front();
}

bool ATPG::tdf_generate_pattern( const fptr fault )
{
  const int origin_backtrack_limit = backtrack_limit;
  const int randFillLen = min(3, detected_num);
  
  // variables
  vector<int>       constraints           ( cktin.size(), U );
  vector<Decision>  decisions_v2;
  string            pattern               ( cktin.size() + 1, itoc( U ) );
  bool              undetected            = false;
  int               current_backtrack_v2  = 0;
  int               useful                = FALSE;
  // end variables

  while( true )
  {
    int backtrack;
    int current_detect_num;

    // generate V2
    backtrack_limit = origin_backtrack_limit - current_backtrack_v2;

    if( !tdf_generate_v2( fault, backtrack, decisions_v2 ) )
    {
      undetected = true;
      break;
    }
    current_backtrack_v2 += backtrack;

    // setup pattern and constraints for v1
    tdf_setup_podem_constraints( constraints );

    pattern.back() = itoc( cktin.front()->value );
    // end setup pattern and constraints for v1
    // end generate V2

    // generate V1
    // the stuck-at fault in v1 should be inverted for podem to generate correct pattern
    backtrack_limit = origin_backtrack_limit;

    if( !tdf_generate_v1( fault, constraints ) )
    {
      if( decisions_v2.empty() ) // all decision assigned or set uniquely implied value
      {
        undetected = true;
        break;
      }
      continue;
    }

    for( size_t i = 0 ; i < cktin.size() ; ++i )
       pattern[i] = itoc( cktin[i]->value );
    // end generate V1

    replace( pattern.begin(), pattern.end(), 'U', '2' );

#if !defined( NDEBUG ) && false
    printf( "pattern: %s\n", pattern.c_str() );
#endif

    if( perform_DTC )
      pattern = tdf_dynamic_compression( fault, pattern );
    
    // random fill unknown;
    vector<string> patterns = randFillPat(pattern, randFillLen);
    for(string pat: patterns) {
        tdfault_sim_a_vector(pat, current_detect_num, useful);
        vectors.push_back(pat);
    }
    
#if !defined( NDEBUG ) && false
    printf( "detect fault: %i, detect num: %i\n", fault->detected_time, current_detect_num );
#endif

    if( fault->detected_time >= detected_num )
      break;
  }
  backtrack_limit = origin_backtrack_limit;

  return !undetected;
}

bool ATPG::tdf_generate_v2( const fptr fault, int &backtrack_num, vector<Decision> &saved_decisions,
                            const vector<int> &constraints )
{
  switch( podem( fault, backtrack_num, constraints, false, saved_decisions ) )
  {
    case FALSE:

#if !defined( NDEBUG ) && false
      printf( "generate v2 fail\n" );
#endif
      if( constraints.empty() )
        fault->detect = REDUNDANT;

    case MAYBE:

#if !defined( NDEBUG ) && false
      printf( "maybe generate v2\n" );
#endif

      return false;
  }

#if !defined( NDEBUG ) && false
  printf( "generate v2\n" );
  display_io();
#endif

  return true;
}

bool ATPG::tdf_generate_v1( const fptr fault, const vector<int> &constraints )
{
  int backtrack;

  fault->fault_type = inverted_fault_type( fault->fault_type );

  switch( podem( fault, backtrack, constraints, true ) )
  {
    case FALSE:

#if !defined( NDEBUG ) && false
      printf( "generate v1 fail\n" );
#endif

    case MAYBE:

      fault->fault_type = inverted_fault_type( fault->fault_type );

#if !defined( NDEBUG ) && false
      printf( "maybe generate v1\n" );
#endif
      return false;
  }

#if !defined( NDEBUG ) && false
      printf( "generate v1\n" );
      display_io();
#endif

  fault->fault_type = inverted_fault_type( fault->fault_type );
  return true;
}

void ATPG::tdf_setup_podem_constraints( vector<int> &constraints )
{
  assert( constraints.size() == cktin.size() ); // precondition

  for( size_t i = 1 ; i < cktin.size() ; ++i )
     constraints[i-1] = cktin[i]->value;

  constraints.back() = U;
}

/*
 *  Duplicate patterns for n detected.
 *  This ensure the fault coverage of test patterns.
 */
void ATPG::tdf_duplicate_pattern()
{
  int useful              = FALSE;
  int current_detect_num;

  auto undetected_faults = flist_undetect;

  for( const fptr fault : undetected_faults )
  {
     if( fault->detected_time == 0 ) continue;

     for( int i = fault->detected_time ; i < detected_num ; ++i )
     {
        tdfault_sim_a_vector( fault->detected_pattern, current_detect_num, useful );
        assert( fault->detected_time == i + 1 );
        vectors.push_back( fault->detected_pattern );
     }
  }
}

string ATPG::tdf_dynamic_compression( const fptr primary_fault, const string &pattern )
{
  // set limit (should be set as flag?)
  const int origin_backtrack_limit = backtrack_limit;
  const int maxSortLen = 200;
  const int sortFreq = 40;

  auto is_not_candidate =
    [&]( const fptr fault )
    {
      return ( sort_wlist[fault->to_swlist]->value != U );
    };

  string              pattern_compressed  = pattern;
  forward_list<fptr>  flist               = tdf_collect_compress_faults( pattern );
  fptr                fault;
  int                 sortCnt             = 0;

#if !defined( NDEBUG ) && false
  printf( "dynamic compressing\n" );
#endif
  
  flist.reverse();
  tdfReRank(flist, pat2cstr(pattern_compressed), maxSortLen, false );
  while(  ( fault = select_a_fault( primary_fault, flist ) ) &&
          ( pattern_compressed.find( '2' ) != string::npos ) )
  {
    vector<int>       constraints( cktin.size(), U );
    vector<Decision>  decisions_v2;
    int               current_backtrack_v2 = 0;

    while( true )
    {
      int backtrack;

      // generate V2
      backtrack_limit = origin_backtrack_limit - current_backtrack_v2;

      for( size_t i = 0 ; i < constraints.size() ; ++i )
         constraints[i] = ctoi( ( i == 0 ) ? pattern_compressed.back() : pattern_compressed[i-1] );

      if( !tdf_generate_v2( fault, backtrack, decisions_v2, constraints ) )
        break;

      flist.remove_if( is_not_candidate );
      current_backtrack_v2 += backtrack;

      // setup pattern_compressed and constraints for v1
      tdf_setup_podem_constraints( constraints );
      constraints.back() = ctoi( pattern_compressed[cktin.size()-1] );

      pattern_compressed.back() = itoc( cktin.front()->value );
      // end setup pattern_compressed and constraints for v1
      // end generate V2

      // generate V1
      // the stuck-at fault in v1 should be inverted for podem to generate correct pattern_compressed
      backtrack_limit = origin_backtrack_limit;

      if( !tdf_generate_v1( fault, constraints ) )
      {
        if( decisions_v2.empty() ) // all decision assigned or set uniquely implied value
          break;
        continue;
      }
      flist.remove_if( is_not_candidate );

      for( size_t i = 0 ; i < cktin.size() ; ++i )
         pattern_compressed[i] = itoc( cktin[i]->value );
      // end generate V1

#if !defined( NDEBUG ) && false
      printf( "compressed\n" );
#endif

      replace( pattern_compressed.begin(), pattern_compressed.end(), 'U', '2' );
      break;
    }
    flist.remove( fault );
    if(++sortCnt == sortFreq) {
        //cout << "reranking: " << pattern_compressed << endl;
        sortCnt = 0;
        tdfReRank(flist, pat2cstr(pattern_compressed), maxSortLen, false );
    }
  }
#if !defined( NDEBUG ) && false
  printf( "before compressed: %s\n", pattern.c_str() );
  printf( "after  compressed: %s\n", pattern_compressed.c_str() );
#endif
  return pattern_compressed;
}

forward_list<ATPG::fptr> ATPG::tdf_collect_compress_faults( const string &pattern )
{
  auto is_not_candidate =
    [&]( const fptr fault )
    {
      return ( sort_wlist[fault->to_swlist]->value != U );
    };

  forward_list<fptr> flist = flist_undetect;

  flist.remove_if(
    []( const fptr fault )
    {
      return ( fault->detect == REDUNDANT );
    }
    );

  // setup faults that can be compressed
  for( size_t i = 0 ; i < cktin.size() ; ++i )
  {
     cktin[i]->value  =   ctoi( pattern[i] );
     cktin[i]->flag   |=  CHANGED;
  }
  sim();

  flist.remove_if( is_not_candidate );

  for( size_t i = 0 ; i < cktin.size() ; ++i )
  {
     cktin[i]->value  =   ctoi( ( i == 0 ) ? pattern.back() : pattern[i-1] );
     cktin[i]->flag   |=  CHANGED;
  }
  sim();

  flist.remove_if( is_not_candidate );
  // end setup faults that can be compressed

  return flist;
}

int inverted_fault_type( int fault_type )
{
  switch( fault_type )
  {
    case STR: return STF;
    case STF: return STR;
    default:  assert( false );
  }
}

void ATPG::tdf_fill_pattern(string& vectors,int& no_of_vectors, int& no_of_detects)
{
  int j,ncktin;
  string pattern;
  ncktin = sort_wlist.size();
  while (no_of_vectors < no_of_detects) {
    for (j = 0; j < ncktin; j++) {
      switch (cktin[j]->value) {
        case 0:
        case B: pattern[j] = '0'; break;
        case 1:
        case D: pattern[j] = '1'; break;
        case U: pattern[j] = '0' + (rand() & 1); break;
        default: printf("something is wrong in tdf_fill_pattern.\n");
      }
    }
    switch (cktin[0]->value2) {
      case 0:
      case B: pattern[ncktin] = '0'; break;
      case 1:
      case D: pattern[ncktin] = '1'; break;
      case U: pattern[ncktin] = '0' + (rand() & 1); break;
      default: printf("something is wrong in tdf_fill_pattern.\n");
    }
    for (j = 0; j < ncktin; j++) {
    vectors[no_of_vectors] = pattern[j];
    no_of_vectors += 1;}
  }
}

void ATPG::tdf_reverse_compression(int& detected_num,int& useful)
{
    int no_of_comp_vectors = 0;
    int i;//detect_num, useful;
    printf("# before compression: %lu\n", vectors.size());
    useful = FALSE;
    for(i = vectors.size() - 1; i >= 0; i--) {
      tdfault_sim_a_vector(vectors[i],detected_num, useful);
        if(useful == TRUE) {
            comp_vectors[no_of_comp_vectors] = vectors[i];
            no_of_comp_vectors += 1;
        }
      /*  else {
            free(vectors[i]);
        }*/
    }
    vectors = comp_vectors;
    in_vector_no = no_of_comp_vectors;
    printf("# after compression: %d\n", in_vector_no);
}

void ATPG:: random_switch_pattern()
{
  size_t i, j;
  string tmp;
  for (i = 0; i < vectors.size(); i++) {
    j = rand() % (i+1);
    tmp = vectors[i];
    vectors[i] = vectors[j];
    vectors[j] = tmp;
  }
}
