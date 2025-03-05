#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include "driver/elevio.h"
#include "stdbool.h"

typedef struct {
    int floor; 
    ButtonType button;
} Order;

Order orderList [N_FLOORS * N_BUTTONS];
int orderCount = 0;

void addOrder(int floor, ButtonType button) {
    for (int i = 0; i < orderCount; i++) {
        if (orderList[i].floor == floor && orderList[i].button == button) {
            return;
        }
    }
    orderList[orderCount].floor = floor;
    orderList[orderCount].button = button;
    orderCount++;
}

void removeOrder(int floor) {
    for (int i = 0; i < orderCount; i++) {
        if (orderList[i].floor == floor) {
            for (int j = i; j < orderCount - 1; j++) {
                orderList[j] = orderList[j + 1];
            }
            orderCount--;
            break;
        }
    }
}

void printOrders() {
    printf("Current orders: \n");
    for (int i = 0; i < orderCount; i++){
        printf("Order %d: Floor %d, Button %d\n", i, orderList[i].floor, orderList[i].button);
    }
}

void updateButtonLamp() {
    for (int f = 0; f < N_FLOORS; f++) {
        for (int b = 0; b < N_BUTTONS; b++) {
            int isOrder = 0;
            for (int i = 0; i < orderCount; i++) {
                if (orderList[i].floor == f && orderList[i].button == b) {
                    isOrder = 1;
                    break;
                }
            }
            elevio_buttonLamp(f, b, isOrder);
        }
    }
}

void StopButton() {
    if (elevio_stopButton()){
        elevio_motorDirection(DIRN_STOP);
        exit(0);
    }
}

int findNextOrder(int currentFloor, MotorDirection direction) {
    if (direction == DIRN_UP || direction == DIRN_STOP) {
        for (int i = 0; i < orderCount; i++) {
            if (orderList[i].floor > currentFloor) {
                return orderList[i].floor;
            }
        }
        // If no orders above, check for orders below
        for (int i = 0; i < orderCount; i++) {
            if (orderList[i].floor < currentFloor) {
                return orderList[i].floor;
            }
        }
    } else if (direction == DIRN_DOWN) {
        for (int i = 0; i < orderCount; i++) {
            if (orderList[i].floor < currentFloor) {
                return orderList[i].floor;
            }
        }
        // If no orders below, check for orders above
        for (int i = 0; i < orderCount; i++) {
            if (orderList[i].floor > currentFloor) {
                return orderList[i].floor;
            }
        }
    }
    return -1; // No orders
}

void handleFloorStop(int floor){
    elevio_motorDirection(DIRN_STOP); 
    elevio_doorOpenLamp(1); 
    nanosleep(&(struct timespec){1,0,},NULL);
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
    for (int i = 0; i < orderCount; i++) {
        if (orderList[i].floor == floor && orderList[i].button == 0 && orderList[i].floor != nextOrder) {
            handleFloorStop(orderList[i].floor);
            elevio_motorDirection(DIRN_UP);
        }
    }
}

void checkUnder(int floor, int nextOrder){
    for (int i = 0; i < orderCount; i++) {
        if (orderList[i].floor == floor && orderList[i].button == 1 && orderList[i].floor != nextOrder) {
            handleFloorStop(orderList[i].floor);
            elevio_motorDirection(DIRN_DOWN);
        }
    }
}

void checkInsideOver(int floor, int nextOrder){
    for (int i = 0; i < orderCount; i++) {
        if (orderList[i].floor == floor && orderList[i].button == 2 && orderList[i].floor != nextOrder) {
            handleFloorStop(orderList[i].floor);
            elevio_motorDirection(DIRN_UP);
        }
    }
}

void checkInsideUnder(int floor, int nextOrder){
    for (int i = 0; i < orderCount; i++) {
        if (orderList[i].floor == floor && orderList[i].button == 2 && orderList[i].floor != nextOrder) {
            handleFloorStop(orderList[i].floor);
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
