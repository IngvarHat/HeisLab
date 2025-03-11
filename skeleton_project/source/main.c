#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include "driver/elevio.h"
#include "stdbool.h"

typedef struct Order {
    int floor; 
    ButtonType button;
    struct Order* next;
} Order;

Order* orderList = NULL;

void addOrder(int floor, ButtonType button) {
    Order* newOrder = (Order*)malloc(sizeof(Order));
    newOrder->floor = floor;
    newOrder->button = button;
    newOrder->next = NULL;

    if (orderList == NULL) {
        orderList = newOrder;
    } else {
        Order* current = orderList;
        while (current->next != NULL) {
            if (current->floor == floor && current->button == button) {
                free(newOrder);
                return;
            }
            current = current->next;
        }
        current->next = newOrder;
    }
}

void removeOrder(int floor) {
    Order* current = orderList;
    Order* previous = NULL;

    while (current != NULL) {
        if (current->floor == floor) {
            if (previous == NULL) {
                orderList = current->next;
            } else {
                previous->next = current->next;
            }
            free(current);
            return;
        }
        previous = current;
        current = current->next;
    }
}

void printOrders() {
    printf("Current orders: \n");
    Order* current = orderList;
    int i = 0;
    while (current != NULL) {
        printf("Order %d: Floor %d, Button %d\n", i, current->floor, current->button);
        current = current->next;
        i++;
    }
}

void updateButtonLamp() {
    for (int f = 0; f < N_FLOORS; f++) {
        for (int b = 0; b < N_BUTTONS; b++) {
            int isOrder = 0;
            Order* current = orderList;
            while (current != NULL) {
                if (current->floor == f && current->button == b) {
                    isOrder = 1;
                    break;
                }
                current = current->next;
            }
            elevio_buttonLamp(f, b, isOrder);
        }
    }
}

void StopButton() {
    if (elevio_stopButton()) {
        elevio_motorDirection(DIRN_STOP);
        elevio_stopLamp(1); // Turn on the stop button light
        while (orderList != NULL) {
            Order* temp = orderList;
            orderList = orderList->next;
            free(temp);
        }
        updateButtonLamp(); // Reset the lights

        int floor = elevio_floorSensor();
        if (floor != -1) { // If the elevator is on a floor
            elevio_doorOpenLamp(1); // Open the doors
            nanosleep(&(struct timespec){3, 0}, NULL); // Wait for 3 seconds
            elevio_doorOpenLamp(0); // Close the doors
        }

        while (elevio_stopButton()) {
            // Keep the elevator stopped while the button is held down
            nanosleep(&(struct timespec){0, 100*1000*1000}, NULL); // Sleep for 100ms
        }

        elevio_stopLamp(0); // Turn off the stop button light
    }
}

int findNextOrder(int currentFloor, MotorDirection direction) {
    if (direction == DIRN_UP || direction == DIRN_STOP) {
        Order* current = orderList;
        while (current != NULL) {
            if (current->floor > currentFloor) {
                return current->floor;
            }
            current = current->next;
        }
        current = orderList;
        while (current != NULL) {
            if (current->floor < currentFloor) {
                return current->floor;
            }
            current = current->next;
        }
    } else if (direction == DIRN_DOWN) {
        Order* current = orderList;
        while (current != NULL) {
            if (current->floor < currentFloor) {
                return current->floor;
            }
            current = current->next;
        }
        current = orderList;
        while (current != NULL) {
            if (current->floor > currentFloor) {
                return current->floor;
            }
            current = current->next;
        }
    }
    return -1; // No orders
}

void handleFloorStop(int floor){
    elevio_motorDirection(DIRN_STOP); 
    elevio_doorOpenLamp(1); 
    nanosleep(&(struct timespec){3,0,},NULL);
    elevio_doorOpenLamp(0); 
    removeOrder(floor); 
    printOrders();
}

void checkButtonPresses(int floor, MotorDirection direction) {
    for(int f = 0; f < N_FLOORS; f++){
        for(int b = 0; b < N_BUTTONS; b++){
            int btnPressed = elevio_callButton(f, b);
            if (btnPressed){
                printf("Button pressed: Floor %d, button %d\n ", f, b);
                addOrder(f, b);
                printOrders();   
                if(floor == f && direction == DIRN_STOP){
                    handleFloorStop(floor);
                } else if (floor == 0 && floor == f){
                    handleFloorStop(floor);
                } else if (floor == 3 && floor == f){
                    handleFloorStop(floor);
                }
            }
        }
    }
}


void checkOver(int floor, int nextOrder){
    for (Order* current = orderList; current != NULL; current = current->next) {
        if (current->floor == floor && current->button == 0 && current->floor != nextOrder) {
            handleFloorStop(current->floor);
            elevio_motorDirection(DIRN_UP);
        }
    }
}

void checkUnder(int floor, int nextOrder){
    for (Order* current = orderList; current != NULL; current = current->next) {
        if (current->floor == floor && current->button == 1 && current->floor != nextOrder) {
            handleFloorStop(current->floor);
            elevio_motorDirection(DIRN_DOWN);
        }
    }
}

void checkInsideOver(int floor, int nextOrder){
    for (Order* current = orderList; current != NULL; current = current->next) {
        if (current->floor == floor && current->button == 2 && current->floor != nextOrder) {
            handleFloorStop(current->floor);
            elevio_motorDirection(DIRN_UP);
        }
    }
}

void checkInsideUnder(int floor, int nextOrder){
    for (Order* current = orderList; current != NULL; current = current->next) {
        if (current->floor == floor && current->button == 2 && current->floor != nextOrder) {
            handleFloorStop(current->floor);
            elevio_motorDirection(DIRN_DOWN);
        }
    }
}

void updateFloorIndicator(floor){
    if(floor >= 0 && floor < 4){
        elevio_floorIndicator(floor);
    }
}

int main(){
    elevio_init();
    int floor = elevio_floorSensor();  
    printf("=== Example Program ===\n");
    printf("Press the stop button on the elevator panel to exit\n");
    bool kalibrering = false;

    elevio_motorDirection(DIRN_DOWN);

    // Oppstart - Flytter til etg 1 (0) bestillinger tas imot 
    while(kalibrering == false){
        updateButtonLamp();
        floor = elevio_floorSensor();
    
        if(floor == 0){
            printf("%d", kalibrering);
            kalibrering = true;
        }

        StopButton();
        updateFloorIndicator(floor);
    }
    
    MotorDirection direction = DIRN_UP;

    while(kalibrering == true){
        floor = elevio_floorSensor();
        elevio_motorDirection(DIRN_STOP);

        if(floor == 0){
            direction = DIRN_UP;
        } else if(floor == N_FLOORS-1){
            direction = DIRN_DOWN;
        }

        checkButtonPresses(floor, direction);
        updateButtonLamp();

        if(elevio_obstruction()){
            elevio_stopLamp(1);
        } else {
            elevio_stopLamp(0);
        }
        
        StopButton();
        
        int nextOrder = findNextOrder(floor, direction);
        while (nextOrder != -1) {
            if (nextOrder > floor) {
                elevio_motorDirection(DIRN_UP);
                while (floor < nextOrder) {
                    floor = elevio_floorSensor();
                    StopButton();
                    checkOver(floor, nextOrder);
                    checkInsideOver(floor, nextOrder);
                    checkButtonPresses(floor, direction);
                    updateButtonLamp();
                    updateFloorIndicator(floor);
                }
                handleFloorStop(floor); 
            } else if (nextOrder < floor) {
                elevio_motorDirection(DIRN_DOWN);
                while (floor > nextOrder || floor == -1) {
                    floor = elevio_floorSensor();
                    StopButton();
                    checkInsideUnder(floor, nextOrder);
                    checkUnder(floor, nextOrder);
                    checkButtonPresses(floor, direction);
                    updateButtonLamp();
                    updateFloorIndicator(floor);
                }
                handleFloorStop(floor); 
            }

            direction = DIRN_STOP;
            elevio_motorDirection(DIRN_STOP);
            removeOrder(nextOrder);
            printOrders();
            nextOrder = findNextOrder(floor, direction);
        }

        nanosleep(&(struct timespec){0, 20*1000*1000}, NULL);
    }

    return 0;
}
