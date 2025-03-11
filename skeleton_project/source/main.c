#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include "driver/elevio.h"
#include <stdbool.h>  // Include this header to use 'bool', 'true', and 'false'

typedef struct {
    int floor;
    ButtonType button;
} Order;

// Pointer to dynamically allocated array of orders
Order *orderList = NULL; 
int orderCount = 0; // Keeps track of the number of orders

// Function to add an order dynamically
void addOrder(int floor, ButtonType button) {
    // Allocate memory for one more order
    orderList = realloc(orderList, (orderCount + 1) * sizeof(Order));
    if (orderList == NULL) {
        printf("Memory allocation failed!\n");
        exit(1);  // Exit if memory allocation fails
    }

    // Add the new order
    orderList[orderCount].floor = floor;
    orderList[orderCount].button = button;
    orderCount++;
}

// Function to remove an order by its floor
void removeOrder(int floor) {
    for (int i = 0; i < orderCount; i++) {
        // Check if the order matches the given floor
        if (orderList[i].floor == floor) {
            // Shift the remaining orders down to fill the gap
            for (int j = i; j < orderCount - 1; j++) {
                orderList[j] = orderList[j + 1];
            }

            // Resize the array to remove the last order (orderCount - 1)
            orderList = realloc(orderList, (orderCount - 1) * sizeof(Order));
            if (orderList == NULL && orderCount > 1) {
                printf("Memory reallocation failed!\n");
                exit(1);  // Exit if memory reallocation fails
            }

            orderCount--;
            break;  // Exit the loop once we find the order to remove
        }
    }
}

// Function to print the current orders
void printOrders() {
    printf("Current orders: \n");
    for (int i = 0; i < orderCount; i++) {
        printf("Order %d: Floor %d, Button %d\n", i, orderList[i].floor, orderList[i].button);
    }
}

// Function to free dynamically allocated memory
void freeOrders() {
    free(orderList);  // Free the dynamically allocated array
    orderList = NULL; // Nullify the pointer for safety
    orderCount = 0;   // Reset the order count
}

// Function to update button lamps based on orders
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

// Function to handle the stop button functionality
void StopButton() {
    if (elevio_stopButton()) {
        elevio_motorDirection(DIRN_STOP);
        elevio_stopLamp(1); // Turn on the stop button light
        orderCount = 0; // Delete all orders
        freeOrders(); // Free the dynamically allocated memory
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

// Function to find the next order based on current floor and direction
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

// Function to handle the floor stop and open doors
void handleFloorStop(int floor) {
    elevio_motorDirection(DIRN_STOP); 
    elevio_doorOpenLamp(1); 
    nanosleep(&(struct timespec){3,0,}, NULL);
    elevio_doorOpenLamp(0); 
    removeOrder(floor);  // Remove the order for the given floor
    printOrders();
}

// Main function to simulate the elevator system
int main() {
    elevio_init(); // Initialize the elevator system
    int floor = elevio_floorSensor();  
    printf("=== Elevator Program ===\n");
    printf("Press the stop button on the elevator panel to exit\n");
    bool kalibrering = false;

    elevio_motorDirection(DIRN_DOWN);

    // Start-up - Move to floor 1 (0), accept orders
    while (!kalibrering) {
        updateButtonLamp();
        floor = elevio_floorSensor();
    
        if (floor == 0) {
            printf("%d\n", kalibrering);
            kalibrering = true;
        }

        StopButton();
    }
    
    MotorDirection direction = DIRN_UP;

    // Main loop - elevator operation
    while (kalibrering) {
        floor = elevio_floorSensor();
        elevio_motorDirection(DIRN_STOP);

        if (floor == 0) {
            direction = DIRN_UP;
        } else if (floor == N_FLOORS-1) {
            direction = DIRN_DOWN;
        }

        updateButtonLamp();

        if (elevio_obstruction()) {
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
                    if (elevio_stopButton()){
                        nextOrder = -1;
                    }
                    checkOver(floor, nextOrder);
                    checkInsideOver(floor, nextOrder);
                    checkButtonPresses(floor, direction);
                    updateButtonLamp();
                }
                handleFloorStop(floor);
            } else if (nextOrder < floor) {
                elevio_motorDirection(DIRN_DOWN);
                while (floor > nextOrder || floor == -1) {
                    floor = elevio_floorSensor();
                    StopButton();
                    updateButtonLamp();
                }
                handleFloorStop(floor);
            }

            direction = DIRN_STOP;
            elevio_motorDirection(DIRN_STOP);
            removeOrder(nextOrder);
            printOrders();
            nextOrder = findNextOrder(floor, direction);
        }

        nanosleep(&(struct timespec){0, 20*1000*1000},  NULL);
    }

    // Clean up the dynamically allocated memory at the end
    freeOrders();

    return 0;
}
