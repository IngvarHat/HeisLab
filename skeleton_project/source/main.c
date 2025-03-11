#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include "driver/elevio.h"


typedef struct {
    int floor; 
    ButtonType button;
} Order;

// Dynamisk array for ordrer
Order *orderList = NULL;
int orderCount = 0;
int orderCapacity = 0;

// Initialiserer den dynamiske orderList med en startkapasitet
void initOrderList() {
    orderCapacity = 10;  // Startkapasitet for 10 ordrer
    orderList = malloc(orderCapacity * sizeof(Order));
    if (orderList == NULL) {
        perror("Klarte ikke å allokere minne for orderList");
        exit(EXIT_FAILURE);
    }
}

// Legger til en ordre dersom den ikke allerede finnes
void addOrder(int floor, ButtonType button) {
    // Sjekk for duplikater
    for (int i = 0; i < orderCount; i++) {
        if (orderList[i].floor == floor && orderList[i].button == button) {
            return;
        }
    }
    // Øker kapasiteten om nødvendig
    if (orderCount == orderCapacity) {
        orderCapacity *= 2;
        Order *temp = realloc(orderList, orderCapacity * sizeof(Order));
        if (temp == NULL) {
            perror("Klarte ikke å reallokere minne for orderList");
            exit(EXIT_FAILURE);
        }
        orderList = temp;
    }
    orderList[orderCount].floor = floor;
    orderList[orderCount].button = button;
    orderCount++;
}

// Fjerner en spesifikk ordre (basert på floor og button) fra den dynamiske arrayen.
// Etter fjerning flyttes de påfølgende elementene, og arrayet reallokeres til ny størrelse.
void removeOrder(int floor, ButtonType button) {
    int index = -1;
    for (int i = 0; i < orderCount; i++) {
        if (orderList[i].floor == floor && orderList[i].button == button) {
            index = i;
            break;
        }
    }
    if (index == -1) return; // Ordren finnes ikke

    // Flytt elementene etter den slettede ett hakk mot starten
    if (index < orderCount - 1) {
        memmove(&orderList[index], &orderList[index + 1], (orderCount - index - 1) * sizeof(Order));
    }
    orderCount--;

    // Realloker arrayet slik at ubrukt minne fjernes.
    if (orderCount > 0) {
        Order *temp = realloc(orderList, orderCount * sizeof(Order));
        if (temp != NULL) {
            orderList = temp;
            orderCapacity = orderCount;
        }
    } else {
        free(orderList);
        orderList = NULL;
        orderCapacity = 0;
    }
}

// Fjerner alle ordrer fra den dynamiske arrayen.
// Etter at alle ordrer er slettet, reallokeres arrayet til en ny tom array slik at vi kan fortsette.
void RemoveAllOrders() {
    free(orderList);
    orderList = NULL;
    orderCount = 0;
    orderCapacity = 0;
    // Reinitialiser arrayet slik at nye ordrer kan legges til
    initOrderList();
    updateButtonLamp();
}

void printOrders() {
    printf("Current orders (%d):\n", orderCount);
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
        elevio_stopLamp(1); // Slå på stop-knappen sitt lys
        RemoveAllOrders(); // Sletter alle ordrer

        int floor = elevio_floorSensor();
        if (floor != -1) { // Dersom heisen er på etasje
            elevio_doorOpenLamp(1); // Åpne dørene
            nanosleep(&(struct timespec){3, 0}, NULL); // Vent 3 sekunder
            elevio_doorOpenLamp(0); // Lukk dørene
        }

        while (elevio_stopButton()) {
            // Hold heisen stoppet så lenge knappen holdes nede
            nanosleep(&(struct timespec){0, 100*1000*1000}, NULL); // Sleep 100ms
        }

        elevio_stopLamp(0); // Slå av stop-knappen sitt lys
    }
}

int findNextOrder(int currentFloor, MotorDirection direction) {
    if (direction == DIRN_UP || direction == DIRN_STOP) {
        for (int i = 0; i < orderCount; i++) {
            if (orderList[i].floor > currentFloor) {
                return orderList[i].floor;
            }
        }
        // Dersom ingen ordrer over, se etter ordrer under
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
        // Dersom ingen ordrer under, se etter ordrer over
        for (int i = 0; i < orderCount; i++) {
            if (orderList[i].floor > currentFloor) {
                return orderList[i].floor;
            }
        }
    }
    return -1; // Ingen ordrer
}

// Håndterer stopp ved et etasjestopp. Fjerner alle ordrer for et gitt etasjen.
void handleFloorStop(int floor){
    elevio_motorDirection(DIRN_STOP); 
    elevio_doorOpenLamp(1); 
    nanosleep(&(struct timespec){3,0}, NULL);
    elevio_doorOpenLamp(0); 
    // Fjerner alle ordrer for denne etasjen (uavhengig av knapp)
    for (int i = orderCount - 1; i >= 0; i--) {
        if (orderList[i].floor == floor) {
            removeOrder(orderList[i].floor, orderList[i].button);
        }
    }
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
                // Dersom heisen er stoppet og vi trykker i riktig etasje, stopp heisen
                if (floor == f && direction == DIRN_STOP){
                    handleFloorStop(floor);
                } else if (floor == 0 && floor == f){
                    handleFloorStop(floor);
                } else if (floor == N_FLOORS - 1 && floor == f){
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

void updateFloorIndicator(int floor){
    if (floor >= 0 && floor < N_FLOORS){
        elevio_floorIndicator(floor);
    }
}

int main(){
    elevio_init();
    initOrderList();  // Initialiserer den dynamiske orderList

    int floor = elevio_floorSensor();  
    printf("=== Example Program ===\n");
    printf("Press the stop button on the elevator panel to exit\n");
    bool kalibrering = false;

    elevio_motorDirection(DIRN_DOWN);

    // Oppstart - flytter til etasje 0 (markert som start) der bestillinger tas imot 
    while(kalibrering == false){
        updateButtonLamp();
        floor = elevio_floorSensor();
    
        if (floor == 0){
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

        if (floor == 0){
            direction = DIRN_UP;
        } else if (floor == N_FLOORS - 1){
            direction = DIRN_DOWN;
        }

        checkButtonPresses(floor, direction);
        updateButtonLamp();

        if (elevio_obstruction()){
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
            // Fjerner alle ordrer for etasjen nextOrder
            for (int i = orderCount - 1; i >= 0; i--) {
                if (orderList[i].floor == nextOrder) {
                    removeOrder(orderList[i].floor, orderList[i].button);
                }
            }
            printOrders();
            nextOrder = findNextOrder(floor, direction);
        }

        nanosleep(&(struct timespec){0, 20 * 1000 * 1000}, NULL);
    }

    free(orderList);
    return 0;
}
