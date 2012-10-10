/*
 * catmouse.c
 *
 * 30-1-2003 : GWA : Stub functions created for CS161 Asst1.
 * 26-11-2007: KMS : Modified to use cat_eat and mouse_eat
 * 21-04-2009: KMS : modified to use cat_sleep and mouse_sleep
 * 21-04-2009: KMS : added sem_destroy of CatMouseWait
 *
 */


/*
 * 
 * Includes
 *
 */

#include <types.h>
#include <lib.h>
#include <test.h>
#include <thread.h>
#include <synch.h>

/*
 * 
 * cat,mouse,bowl simulation functions defined in bowls.c
 *
 * For Assignment 1, you should use these functions to
 *  make your cats and mice eat from the bowls.
 * 
 * You may *not* modify these functions in any way.
 * They are implemented in a separate file (bowls.c) to remind
 * you that you should not change them.
 *
 * For information about the behaviour and return values
 *  of these functions, see bowls.c
 *
 */

/* this must be called before any calls to cat_eat or mouse_eat */
extern int initialize_bowls(unsigned int bowlcount);

extern void cleanup_bowls( void );
extern void cat_eat(unsigned int bowlnumber);
extern void mouse_eat(unsigned int bowlnumber);
extern void cat_sleep(void);
extern void mouse_sleep(void);

/*
 *
 * Problem parameters
 *
 * Values for these parameters are set by the main driver
 *  function, catmouse(), based on the problem parameters
 *  that are passed in from the kernel menu command or
 *  kernel command line.
 *
 * Once they have been set, you probably shouldn't be
 *  changing them.
 *
 *
 */
int NumBowls;  // number of food bowls
int NumCats;   // number of cats
int NumMice;   // number of mice
int NumLoops;  // number of times each cat and mouse should eat

volatile int NumCatsEating;
volatile int NumMiceEating;
unsigned long Serving;
struct lock *EatingMutex, *BowlMutex;
struct cv *CatEating, *MouseEating, *BowlSignal;

/*
 * Once the main driver function (catmouse()) has created the cat and mouse
 * simulation threads, it uses this semaphore to block until all of the
 * cat and mouse simulations are finished.
 */
struct semaphore *CatMouseWait;

/*
 * 
 * Function Definitions
 * 
 */

static
unsigned int
getOpenBowl(char *bowlflag)
{
	int i;
	unsigned int open_bowl = 0;
	
	lock_acquire(BowlMutex);	
	while(open_bowl == 0){		
		for(i=0; i<NumBowls; i++){
			if(bowlflag[i] == 'o'){
				bowlflag[i] = 'f';
				open_bowl = i + 1;
				break; //found an open bowl leave for loop
			}
		}
		
		if(open_bowl == 0){
			cv_wait(BowlSignal, BowlMutex);
		}
	}
	lock_release(BowlMutex);
	
	return open_bowl;
}

/*
 * cat_simulation()
 *
 * Arguments:
 *      void * unusedpointer: currently unused.
 *      unsigned long catnumber: holds cat identifier from 0 to NumCats-1.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Each cat simulation thread runs this function.
 *
 *      You need to finish implementing this function using 
 *      the OS161 synchronization primitives as indicated
 *      in the assignment description
 */

static
void
cat_simulation(void * pointer, 
	       unsigned long catnumber)
{
  int i;
  unsigned int bowl;
  char *bowlstate = (char*)pointer;
  
  /* your simulated cat must iterate NumLoops times,
   * sleeping (by calling cat_sleep() and eating
   * (by calling cat_eat()) on each iteration */
  for(i=0;i<NumLoops;i++) {

	/* do not synchronize calls to cat_sleep().
	 * Any number of cats (and mice) should be able
	 * sleep at the same time. */
    cat_sleep();

	/* for now, this cat chooses a random bowl from
     * which to eat, and it is not synchronized with
     * other cats and mice.
     *
     * you will probably want to control which bowl this
     * cat eats from, and you will need to provide 
     * synchronization so that the cat does not violate
     * the rules when it eats */

	/* legal bowl numbers range from 1 to NumBowls */
    lock_acquire(EatingMutex);
	while(NumMiceEating > 0 || Serving != catnumber){
		cv_wait(MouseEating, EatingMutex);
	}
	assert(NumMiceEating == 0);
	assert(Serving == catnumber);
			
	NumCatsEating++;
	Serving++;
	cv_broadcast(MouseEating, EatingMutex);
    lock_release(EatingMutex);
    
	bowl = getOpenBowl(bowlstate);
    cat_eat(bowl);

	lock_acquire(BowlMutex);
	bowlstate[bowl-1] = 'o';
	cv_signal(BowlSignal, BowlMutex);
	lock_release(BowlMutex);

	lock_acquire(EatingMutex);
	NumCatsEating--;

	if(NumCatsEating == 0){
		Serving = 0;
		cv_broadcast(CatEating, EatingMutex);
	}
    lock_release(EatingMutex);
  }

  /* indicate that this cat simulation is finished */
  V(CatMouseWait); 
}
	
/*
 * mouse_simulation()
 *
 * Arguments:
 *      void * unusedpointer: currently unused.
 *      unsigned long mousenumber: holds mouse identifier from 0 to NumMice-1.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      each mouse simulation thread runs this function
 *
 *      You need to finish implementing this function using 
 *      the OS161 synchronization primitives as indicated
 *      in the assignment description
 *
 */

static
void
mouse_simulation(void * pointer,
          unsigned long mousenumber)
{
  int i;
  unsigned int bowl;
  char *bowlstate = (char*)pointer;

  /* your simulated mouse must iterate NumLoops times,
   *  sleeping (by calling mouse_sleep()) and eating
   *  (by calling mouse_eat()) on each iteration */
  for(i=0;i<NumLoops;i++) {

    /* do not synchronize calls to mouse_sleep().
       Any number of mice (and cats) should be able
       sleep at the same time. */
    mouse_sleep();

    /* for now, this mouse chooses a random bowl from
     * which to eat, and it is not synchronized with
     * other cats and mice.
     *
     * you will probably want to control which bowl this
     * mouse eats from, and you will need to provide 
     * synchronization so that the mouse does not violate
     * the rules when it eats */


    lock_acquire(EatingMutex);
	while(NumCatsEating > 0 || Serving != mousenumber){
		cv_wait(CatEating, EatingMutex);
	}
	assert(NumCatsEating == 0);
	assert(Serving == mousenumber);
		
	NumMiceEating++;
	Serving++;
	cv_broadcast(CatEating, EatingMutex);
    lock_release(EatingMutex);
    
    bowl = getOpenBowl(bowlstate);
    mouse_eat(bowl);
    
    lock_acquire(BowlMutex);
	bowlstate[bowl-1] = 'o';
	cv_signal(BowlSignal, BowlMutex);
	lock_release(BowlMutex);

    lock_acquire(EatingMutex);
	NumMiceEating--;
	if(NumMiceEating == 0){
		Serving = 0;
		cv_broadcast(MouseEating, EatingMutex);
	}
    lock_release(EatingMutex);
  }

  /* indicate that this mouse is finished */
  V(CatMouseWait); 
}


/*
 * catmouse()
 *
 * Arguments:
 *      int nargs: should be 5
 *      char ** args: args[1] = number of food bowls
 *                    args[2] = number of cats
 *                    args[3] = number of mice
 *                    args[4] = number of loops
 *
 * Returns:
 *      0 on success.
 *
 * Notes:
 *      Driver code to start up cat_simulation() and
 *      mouse_simulation() threads.
 *      You may need to modify this function, e.g., to
 *      initialize synchronization primitives used
 *      by the cat and mouse threads.
 *      
 *      However, you should should ensure that this function
 *      continues to create the appropriate numbers of
 *      cat and mouse threads, to initialize the simulation,
 *      and to wait for all cats and mice to finish.
 */

int
catmouse(int nargs,
	 char ** args)
{
  int index, error;
  int i;

  /* check and process command line arguments */
  if (nargs != 5) {
    kprintf("Usage: <command> NUM_BOWLS NUM_CATS NUM_MICE NUM_LOOPS\n");
    return 1;  // return failure indication
  }

  /* check the problem parameters, and set the global variables */
  NumBowls = atoi(args[1]);
  if (NumBowls <= 0) {
    kprintf("catmouse: invalid number of bowls: %d\n",NumBowls);
    return 1;
  }
  NumCats = atoi(args[2]);
  if (NumCats < 0) {
    kprintf("catmouse: invalid number of cats: %d\n",NumCats);
    return 1;
  }
  NumMice = atoi(args[3]);
  if (NumMice < 0) {
    kprintf("catmouse: invalid number of mice: %d\n",NumMice);
    return 1;
  }
  NumLoops = atoi(args[4]);
  if (NumLoops <= 0) {
    kprintf("catmouse: invalid number of loops: %d\n",NumLoops);
    return 1;
  }
  kprintf("Using %d bowls, %d cats, and %d mice. Looping %d times.\n",
          NumBowls,NumCats,NumMice,NumLoops);

  /* create the semaphore that is used to make the main thread
     wait for all of the cats and mice to finish */
  CatMouseWait = sem_create("CatMouseWait",0);
  if (CatMouseWait == NULL) {
    panic("catmouse: could not create semaphore\n");
  }

  /* 
   * initialize the bowls
   */
  if (initialize_bowls(NumBowls)) {
    panic("catmouse: error initializing bowls.\n");
  }

  EatingMutex = lock_create("EatingMutex");
  if(EatingMutex == NULL){
    panic("catmouse:error creating eating mutex");
  }
  
  CatEating = cv_create("CatEating");
  if(CatEating == NULL){
    panic("catmouse:error creating cat tracker");
  }
  
  MouseEating = cv_create("MouseEating");
  if(MouseEating == NULL){
    panic("catmouse:error creating mouse tracker");  
  }

  BowlMutex = lock_create("BowlMutex");
  if(BowlMutex == NULL){
    panic("catmouse:error creating bowl mutex");
  }

  Serving = 0;
  NumCatsEating = 0;
  NumMiceEating = 0;
  
  char BowlState[NumBowls];
  for(i=0;i<NumBowls;i++){
	  BowlState[i] = 'o';
  }

  BowlSignal = cv_create("BowlSignal");
  if(BowlSignal == NULL){
	  panic("catmouse:error creating bowlsignal");
  }

  /*
   * Start NumCats cat_simulation() threads.
   */
  for (index = 0; index < NumCats; index++) {
    error = thread_fork("cat_simulation thread",BowlState,index,cat_simulation,NULL);
    if (error) {
      panic("cat_simulation: thread_fork failed: %s\n", strerror(error));
    }
  }

  /*
   * Start NumMice mouse_simulation() threads.
   */
  for (index = 0; index < NumMice; index++) {
    error = thread_fork("mouse_simulation thread",BowlState,index,mouse_simulation,NULL);
    if (error) {
      panic("mouse_simulation: thread_fork failed: %s\n",strerror(error));
    }
  }

  /* wait for all of the cats and mice to finish before
     terminating */  
  for(i=0;i<(NumCats+NumMice);i++) {
    P(CatMouseWait);
  }

  /* clean up the semaphore that we created */
  sem_destroy(CatMouseWait);
  lock_destroy(EatingMutex);
  lock_destroy(BowlMutex);
  cv_destroy(CatEating);
  cv_destroy(MouseEating);
  cv_destroy(BowlSignal);
  /* clean up resources used for tracking bowl use */
  cleanup_bowls();

  return 0;
}

/*
 * End of catmouse.c
 */
