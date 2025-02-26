#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include "driver/elevio.h"
#include "stdbool.h"

//Test


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

void removeOrder(int orderIndex){
    if (orderIndex <0 || orderIndex >= orderCount){
        return; 
    }

    for (int i=orderIndex; i<orderCount-1; i++){
        orderList[i]=orderList[i+1]; 
    }

    orderCount--;
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

int main(){
    elevio_init();
    int floor = elevio_floorSensor();  
    printf("=== Example Program ===\n");
    printf("Press the stop button on the elevator panel to exit\n");
    bool kalibrering = false;

    elevio_motorDirection(DIRN_DOWN);


    //Oppstart - Flytter til etg 1 (0) bestillinger tas imot 
    while(kalibrering == false){
        updateButtonLamp();
        floor = elevio_floorSensor();
    
        if(floor == 0){
            printf("%d", kalibrering);
            kalibrering = true;
        }

        if(kalibrering == true){
        }

        StopButton();
        if(floor >= 0 && floor < 4){
            elevio_floorIndicator(floor);
        }


    }
    
  
    while(kalibrering == true){
        //printf("%d", kalibrering);
        floor = elevio_floorSensor();
        // Node *bestillingsliste =NULL; //Setter bestillingsliste 0

        //while(bestillingsliste == NULL){
        //    elevio_motorDirection(DIRN_STOP);
        //    StopButton();
        //}
        elevio_motorDirection(DIRN_STOP);

        for(int f=0; f<N_FLOORS; f++){
            for(int b=0; b < N_BUTTONS; b++){
                for(int i=0; i<orderCount; i++){
                    if(orderList[i].floor>floor){
                        while(orderList[i].floor>floor){
                            floor = elevio_floorSensor();
                            elevio_motorDirection(DIRN_UP);
                            StopButton();
                        if (orderList[i].floor==floor){
                            elevio_motorDirection(DIRN_STOP); 
                            void removeOrder(int i); 
                            printOrders(); 
                        }
                    }
                         
                        
                    }

                }
            }
        }

        if(floor == 0){
            elevio_motorDirection(DIRN_UP);
        }

        if(floor == N_FLOORS-1){
            elevio_motorDirection(DIRN_DOWN);
        }


        if(floor >= 0 && floor < 4){
            elevio_floorIndicator(floor);
        }

        for(int f = 0; f < N_FLOORS; f++){
            for(int b = 0; b < N_BUTTONS; b++){
                int btnPressed = elevio_callButton(f, b);
                if (btnPressed){
                    printf("Button pressed: Floor %d, button %d\n ", f, b);
                    addOrder(f, b);
                    printOrders();   
                }
            }
        }

        updateButtonLamp();

        if(elevio_obstruction()){
            elevio_stopLamp(1);
        } else {
            elevio_stopLamp(0);
        }
        
        StopButton();
        
        nanosleep(&(struct timespec){0, 20*1000*1000}, NULL);
    }

    return 0;
}
