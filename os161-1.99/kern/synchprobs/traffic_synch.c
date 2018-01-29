#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
//static struct semaphore *intersectionSem;




//Assignment 1b

static struct lock *intersection_lock;
static struct cv *intersection_cv;

/*  north = 0,
 *  east = 1,
 *  south = 2,
 *  west = 3
*/

/* this is a sum of how many cars in that intersection, 
      if the "." changed to a num"a" it means there are "a" cars in that intersetion and it comes from x and going to y.
  x/y  0 1 2 3
  0    . . . .
  1    . . . .
  2    . . . .
  3    . . . .
*/
static volatile int car_direction [4][4];
void new_car(void);

//there are three cases must be true
/*
1. Va and Vb entered the intersection from the same direction, 
      i.e., Va.origin = Vb.origin, or
2. Va and Vb are going in opposite directions, 
      i.e., Va.origin = Vb.destination and Va.destination = Vb.origin, or
3. Va and Vb have different destinations, and at least one of them is making a right turn, 
      e.g., Va is right-turning from north to west, and Vb is going from south to north.
*/
bool same_direction(Direction, Direction);
bool opposite_direction(Direction, Direction, Direction, Direction);
bool right_turning(Direction, Direction);
bool different_destinations(Direction, Direction, Direction, Direction);
bool safely_share (Direction, Direction);


void new_car(void){
  for(int x=0; x<4; x++){
    for(int y=0; y<4; y++){
      car_direction[x][y] = 0;
    }
  }
}

bool same_direction(Direction origin1, Direction origin2){
  if(origin1 == origin2){
    return true;
  }
  return false;
}

bool opposite_direction(Direction origin1, Direction origin2, Direction destination1, Direction destination2){
  if(origin1 == destination2 && origin2 == destination1){
    return true;
  }
  return false;
}

bool right_turning(Direction origin, Direction destination){
  if(origin-1 == destination){
    return true;
  }
  if(origin == 0 && destination == 3){
    return true;
  }
  return false;
}

bool different_destinations(Direction origin1, Direction origin2, Direction destination1, Direction destination2){
  if(destination2 != destination1){
    if(right_turning( origin1,  destination1)){
      return true;
    }
    if(right_turning( origin2,  destination2)){
      return true;
    }
  }
  return false;
}

bool safely_share (Direction origin, Direction destination){
  for(int x=0; x<4; x++){
    for(int y=0; y<4; y++){
      if(car_direction[x][y]>0){
        if(!(same_direction(origin, x) || 
          opposite_direction(origin, x, destination, y) ||
          different_destinations(origin, x, destination, y))){
          return false;
        }
      }
    }
  }
  return true;
}


/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void
intersection_sync_init(void)
{
  /* replace this default implementation with your own implementation */

  //intersectionSem = sem_create("intersectionSem",1);
  //if (intersectionSem == NULL) {
  //  panic("could not create intersection semaphore");
  //}
  intersection_lock = lock_create("intersection_lock");
  if(intersection_lock == NULL){
    panic("could not create intersection lock");
  }
  intersection_cv = cv_create("intersection_cv");
  if(intersection_cv == NULL){
    panic("could not create intersection cv");
  }
  new_car();
  return;
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  /* replace this default implementation with your own implementation */
  //KASSERT(intersectionSem != NULL);
  //sem_destroy(intersectionSem);
  KASSERT(intersection_lock != NULL);
  KASSERT(intersection_cv != NULL);
  lock_destroy(intersection_lock);
  cv_destroy(intersection_cv);
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{
  /* replace this default implementation with your own implementation */
  (void)origin;  /* avoid compiler complaint about unused parameter */
  (void)destination; /* avoid compiler complaint about unused parameter */
  //KASSERT(intersectionSem != NULL);
  //P(intersectionSem);
  KASSERT(intersection_lock != NULL);
  KASSERT(intersection_cv != NULL);
  lock_acquire(intersection_lock);
  //the car cannot enter the intersection, so wait
  while(!safely_share(origin,destination)){
    cv_wait(intersection_cv,intersection_lock);
  }
  //finished while loop, i.e teh car can go in to the intersection now.
  car_direction[origin][destination]++;
  lock_release(intersection_lock);
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
  /* replace this default implementation with your own implementation */
  (void)origin;  /* avoid compiler complaint about unused parameter */
  (void)destination; /* avoid compiler complaint about unused parameter */
  //KASSERT(intersectionSem != NULL);
  //V(intersectionSem);
  KASSERT(intersection_lock != NULL);
  KASSERT(intersection_cv != NULL);
  lock_acquire(intersection_lock);
  car_direction[origin][destination]--;
  cv_signal(intersection_cv,intersection_lock);
  lock_release(intersection_lock);
}
