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

Order orderList[N_FLOORS * N_BUTTONS];
int orderCount = 0;

// Global peker til neste bestilling.
Order* nextOrder = NULL;

void addOrder(int floor, ButtonType button) {
    for (int i = 0; i < orderCount; i++) {
        if (orderList[i].floor == floor && orderList[i].button == button) {
            return;
        }
    }
    orderList[orderCount].floor = floor;
    orderList[orderCount].button = button;
    orderCount++;
    printf("Ordercount: %d\n", orderCount); 
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
    if (elevio_stopButton()) {
        elevio_motorDirection(DIRN_STOP);
        elevio_stopLamp(1); // Slå på stopplampen
        orderCount = 0; // Fjern alle bestillinger
        updateButtonLamp(); // Nullstill lampene

        int floor = elevio_floorSensor();
        if (floor != -1) { // Hvis heisen er på en etasje
            elevio_doorOpenLamp(1); // Åpne dørene
            nanosleep(&(struct timespec){3, 0}, NULL); // Vent i 3 sekunder
            elevio_doorOpenLamp(0); // Lukk dørene
        }

        while (elevio_stopButton()) {
            nanosleep(&(struct timespec){0, 100 * 1000 * 1000}, NULL); // Sleep i 100ms
        }

        elevio_stopLamp(0); // Slå av stopplampen

        // Sett nextOrder til NULL ved stopp
        nextOrder = NULL;
    }
}

Order* findNextOrder(int currentFloor, MotorDirection direction) {
    if (direction == DIRN_UP || direction == DIRN_STOP) {
        for (int i = 0; i < orderCount; i++) {
            if (orderList[i].floor > currentFloor) {
                return &orderList[i];
            }
        }
        // Hvis ingen bestillinger over, sjekk bestillinger under
        for (int i = 0; i < orderCount; i++) {
            if (orderList[i].floor < currentFloor) {
                return &orderList[i];
            }
        }
    } else if (direction == DIRN_DOWN) {
        for (int i = 0; i < orderCount; i++) {
            if (orderList[i].floor < currentFloor) {
                return &orderList[i];
            }
        }
        // Hvis ingen bestillinger under, sjekk bestillinger over
        for (int i = 0; i < orderCount; i++) {
            if (orderList[i].floor > currentFloor) {
                return &orderList[i];
            }
        }
    }
    return NULL; // Ingen bestillinger funnet
}

void handleFloorStop(int floor){
    elevio_motorDirection(DIRN_STOP); 
    elevio_doorOpenLamp(1); 
    nanosleep(&(struct timespec){3, 0}, NULL);
    elevio_doorOpenLamp(0); 
    removeOrder(floor); 
    printOrders();
}

void checkButtonPresses(int floor, MotorDirection direction) {
    for (int f = 0; f < N_FLOORS; f++){
        for (int b = 0; b < N_BUTTONS; b++){
            int btnPressed = elevio_callButton(f, b);
            if (btnPressed){
                printf("Button pressed: Floor %d, button %d\n", f, b);
                addOrder(f, b);
                printOrders();   
                if (floor == f && direction == DIRN_STOP) {
                    handleFloorStop(floor);
                } else if (floor == 0 && floor == f) {
                    handleFloorStop(floor);
                } else if (floor == 3 && floor == f) {
                    handleFloorStop(floor);
                }
            }
        }
    }
}

void checkOver(int floor, int nextOrderFloor){
    for (int i = 0; i < orderCount; i++) {
        if (orderList[i].floor == floor && orderList[i].button == 0 && orderList[i].floor != nextOrderFloor) {
            handleFloorStop(orderList[i].floor);
            elevio_motorDirection(DIRN_UP);
        }
    }
}

void checkUnder(int floor, int nextOrderFloor){
    for (int i = 0; i < orderCount; i++) {
        if (orderList[i].floor == floor && orderList[i].button == 1 && orderList[i].floor != nextOrderFloor) {
            handleFloorStop(orderList[i].floor);
            elevio_motorDirection(DIRN_DOWN);
        }
    }
}

void checkInsideOver(int floor, int nextOrderFloor){
    for (int i = 0; i < orderCount; i++) {
        if (orderList[i].floor == floor && orderList[i].button == 2 && orderList[i].floor != nextOrderFloor) {
            handleFloorStop(orderList[i].floor);
            elevio_motorDirection(DIRN_UP);
        }
    }
}

void checkInsideUnder(int floor, int nextOrderFloor){
    for (int i = 0; i < orderCount; i++) {
        if (orderList[i].floor == floor && orderList[i].button == 2 && orderList[i].floor != nextOrderFloor) {
            handleFloorStop(orderList[i].floor);
            elevio_motorDirection(DIRN_DOWN);
        }
    }
}

void updateFloorIndicator(int floor){
    if (floor >= 0 && floor < 4){
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

    // Oppstart: Flytter til etasje 1 (0) og tar imot bestillinger 
    while (!kalibrering) {
        updateButtonLamp();
        floor = elevio_floorSensor();
    
        if (floor == 0) {
            kalibrering = true;
        }
        StopButton();
        updateFloorIndicator(floor);
    }
    
    MotorDirection direction = DIRN_UP;

    // Endret hovedløkke til en uendelig løkke slik at vi stadig sjekker for nye ordrer
    while (1) {
        floor = elevio_floorSensor();
        elevio_motorDirection(DIRN_STOP);

        if (floor == 0) {
            direction = DIRN_UP;
        } else if (floor == N_FLOORS - 1) {
            direction = DIRN_DOWN;
        }

        checkButtonPresses(floor, direction);
        updateButtonLamp();

        if (elevio_obstruction()) {
            elevio_stopLamp(1);
        } else {
            elevio_stopLamp(0);
        }
        
        StopButton();
        
        // Søk etter en ny bestilling, selv om nextOrder kan være NULL etter et stopp
        nextOrder = findNextOrder(floor, direction);
        if (nextOrder != NULL) {
            if (nextOrder->floor > floor) {
                elevio_motorDirection(DIRN_UP);
                while (floor < nextOrder->floor) {
                    floor = elevio_floorSensor();
                    StopButton();
                    checkOver(floor, nextOrder->floor);
                    checkInsideOver(floor, nextOrder->floor);
                    checkButtonPresses(floor, direction);
                    updateButtonLamp();
                    updateFloorIndicator(floor);
                }
                handleFloorStop(floor); 
            } else if (nextOrder->floor < floor) {
                elevio_motorDirection(DIRN_DOWN);
                while (floor > nextOrder->floor || floor == -1) {
                    floor = elevio_floorSensor();
                    StopButton();
                    checkInsideUnder(floor, nextOrder->floor);
                    checkUnder(floor, nextOrder->floor);
                    checkButtonPresses(floor, direction);
                    updateButtonLamp();
                    updateFloorIndicator(floor);
                }
                handleFloorStop(floor); 
            }
            removeOrder(nextOrder->floor);
            printOrders();
        }
        
        nanosleep(&(struct timespec){0, 20 * 1000 * 1000}, NULL);
    }

    return 0;
}
