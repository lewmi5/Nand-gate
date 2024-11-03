#include "nand.h"
#include <errno.h>
#include <malloc.h>

typedef struct input input_t;
struct input {
    bool isSet;
    bool const *signal;
    nand_t *gate;
};

typedef struct list list_t;
struct list {
    nand_t *gate;
    list_t *next;
};

struct nand {
    //inputs
    unsigned int numOfInputs;
    input_t *inputsArr;

    //Gates to which current gate is connected to
    list_t *dummyOutputGatesList;

    //Evaluation
    bool wasVisited;
};

// Creates new gate with n inputs.
nand_t *nand_new(unsigned n) {
    nand_t *a = (nand_t *) malloc(sizeof(nand_t));

    if (a == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    //Inputs
    a->numOfInputs = n;
    if (n != 0) {
        a->inputsArr = (input_t *) malloc(a->numOfInputs * sizeof(input_t));

        if (a->inputsArr == NULL) {
            free(a);
            errno = ENOMEM;
            return NULL;
        }

        for (unsigned int i = 0; i < n; ++i) {
            a->inputsArr[i].isSet = false;
            a->inputsArr[i].signal = NULL;
            a->inputsArr[i].gate = NULL;
        }
    } else {
        a->inputsArr = NULL;
    }

    //Output gates
    a->dummyOutputGatesList = malloc(sizeof(list_t));
    if (a->dummyOutputGatesList == NULL) {
        free(a->inputsArr);
        free(a);
        errno = ENOMEM;
        return NULL;
    }
    a->dummyOutputGatesList->next = NULL;
    a->dummyOutputGatesList->gate = NULL;

    //Evaluation
    a->wasVisited = false;

    return a;
}

// Removes one occurence of 'toDelete' in g->dummyOutputGatesList.
void nand_remove_one_from_nand_outputs_list(nand_t *g, nand_t *toDelete) {
    list_t *current = g->dummyOutputGatesList->next;
    list_t *prev = g->dummyOutputGatesList;

    while (current != NULL) {
        if (current->gate == toDelete) {
            prev->next = current->next;
            free(current);
            return;
        } else {
            prev = current;
            current = current->next;
        }
    }
}


// Removes all occurrences of 'toDelete' in g->dummyOutputGatesList.
void nand_remove_all_from_nand_outputs_list(nand_t *g, nand_t *toDelete) {
    list_t *current = g->dummyOutputGatesList->next;
    list_t *prev = g->dummyOutputGatesList;

    while (current != NULL) {
        if (current->gate == toDelete) {
            prev->next = current->next;
            free(current);
            current = prev->next;
        } else {
            prev = current;
            current = current->next;
        }
    }
}

// Removes all occurences of 'toDelete' in g->inputsArr.
void nand_remove_all_from_nand_inputs_array(nand_t *g, nand_t *toDelete) {
    for (unsigned int i = 0; i < g->numOfInputs; ++i) {
        if (g->inputsArr[i].isSet == true && g->inputsArr[i].gate != NULL
        && g->inputsArr[i].gate == toDelete) {
            g->inputsArr[i].gate = NULL;
            g->inputsArr[i].isSet = false;
        }
    }
}

// Deletes gate and deallocate memory.
void nand_delete(nand_t *g) {
    if (g != NULL) {
        // Remove all inputs of g from other's gates dummyOutputGatesList.
        for (unsigned int i = 0; i < g->numOfInputs; ++i) {
            if (g->inputsArr[i].isSet == true && g->inputsArr[i].gate != NULL) {
                nand_remove_all_from_nand_outputs_list(g->inputsArr[i].gate, g);
            }
        }

        // From gates in dummyOutputGatesList
        // removes all occurrences of 'toDelete' in gate->inputsArr.
        list_t *current = g->dummyOutputGatesList->next;
        while (current != NULL) {
            nand_remove_all_from_nand_inputs_array(current->gate, g);
            current = current->next;
        }

        //Free memory allocated for dummyOutputGatesList
        current = g->dummyOutputGatesList;
        list_t *tmp;
        while (current != NULL) {
            tmp = current->next;
            free(current);
            current = tmp;
        }

        free(g->inputsArr);
        free(g);
    }
}

// Connects output of g_out to k-th input of g_in.
int nand_connect_nand(nand_t *g_out, nand_t *g_in, unsigned k) {
    if (g_out == NULL || g_in == NULL || k >= g_in->numOfInputs) {
        errno = EINVAL;
        return -1;
    }

    // Unplugs gate connected to k-th input.
    if (g_in->inputsArr[k].isSet == true && g_in->inputsArr[k].gate != NULL) {
        nand_remove_one_from_nand_outputs_list(g_in->inputsArr[k].gate, g_in);
    }

    // Plugs g_out to k-th input of g_in.
    g_in->inputsArr[k].gate = g_out;
    g_in->inputsArr[k].isSet = true;
    g_in->inputsArr[k].signal = NULL;

    // Adding g_in to the end of g_out->dummyOutputGatesList.
    list_t *newNode = malloc(sizeof(list_t));
    if (newNode == NULL) {
        errno = ENOMEM;
        return -1;
    }
    newNode->gate = g_in;
    newNode->next = NULL;

    list_t *current = g_out->dummyOutputGatesList;
    while (current->next != NULL)
        current = current->next;

    current->next = newNode;

    return 0;
}

// Connects signal s to k-th input of g.
int nand_connect_signal(bool const *s, nand_t *g, unsigned k) {
    if (s == NULL || g == NULL || k >= g->numOfInputs) {
        errno = EINVAL;
        return -1;
    }

    //Removes one occurrence of g in  g->inputsArr[k].gate->dummyOutputGatesList.
    if (g->inputsArr[k].isSet == true && g->inputsArr[k].gate != NULL) {
        nand_remove_one_from_nand_outputs_list(g->inputsArr[k].gate, g);
    }

    g->inputsArr[k].isSet = true;
    g->inputsArr[k].gate = NULL;
    g->inputsArr[k].signal = s;

    return 0;
}

// Recursive function that returns length of critical path
// and sets evaluatedValue to output of considered gate.
ssize_t nand_evaluate_helper(nand_t *g, bool *const evaluatedValue) {
    if (g->wasVisited == true) {
        errno = ECANCELED;
        g->wasVisited = false;
        return -1;
    }

    g->wasVisited = true;

    if (g->numOfInputs == 0) {
        *evaluatedValue = false;
        g->wasVisited = false;
        return 0;
    }

    unsigned int k = 0;
    bool everyInputWasTrue = true;
    bool evaluationOfKthInput;
    ssize_t tmp;
    ssize_t longestPath = 0;

    while (k < g->numOfInputs) {
        if (g->inputsArr[k].isSet == false) {
            errno = ECANCELED;
            g->wasVisited = false;
            return -1;
        }

        if (g->inputsArr[k].gate != NULL) { //Gate
            tmp = nand_evaluate_helper(g->inputsArr[k].gate, &evaluationOfKthInput);

            if (tmp == -1) {
                g->wasVisited = false;
                return -1;
            }

            if (tmp > longestPath) {
                longestPath = tmp;
            }
        } else { //Signal
            evaluationOfKthInput = *(g->inputsArr[k].signal);
        }

        if (evaluationOfKthInput == false)
            everyInputWasTrue = false;

        k++;
    }

    g->wasVisited = false;

    if (everyInputWasTrue)
        *evaluatedValue = false;
    else
        *evaluatedValue = true;

    return longestPath + 1;
}

// Returns length of critical path and calculates logic values on gates in s.
ssize_t nand_evaluate(nand_t **g, bool *s, size_t m) {
    if (g == NULL || s == NULL || m == 0) {
        errno = EINVAL;
        return -1;
    }

    for (size_t i = 0; i < m; ++i) {
        if (g[i] == NULL) {
            errno = EINVAL;
            return -1;
        }
    }

    ssize_t longestPath = 0;
    ssize_t tmp;
    bool evaluation;

    for (size_t i = 0; i < m; ++i) {
        tmp = nand_evaluate_helper(g[i], &evaluation);
        if (tmp == -1)
            return -1;
        if (tmp > longestPath)
            longestPath = tmp;

        s[i] = evaluation;
    }

    return longestPath;
}

// Returns number of gates to which g is connected to.
ssize_t nand_fan_out(nand_t const *g) {
    if (g == NULL) {
        errno = EINVAL;
        return -1;
    }

    list_t *current = g->dummyOutputGatesList->next;
    ssize_t counter = 0;

    while (current != NULL) {
        counter++;
        current = current->next;
    }

    return counter;
}

// Returns pointer to k-th input of g.
// It is either a signal or a gate.
void *nand_input(nand_t const *g, unsigned k) {
    if (g == NULL || k >= g->numOfInputs) {
        errno = EINVAL;
        return NULL;
    }

    if (g->inputsArr[k].isSet == false) {
        errno = 0;
        return NULL;
    }

    if (g->inputsArr[k].gate != NULL) {
        return g->inputsArr[k].gate;
    } else
        return (bool *) g->inputsArr[k].signal;
}

// It allows to iterate through gates that g is connected to.
// Returns k-th gate from these.
nand_t *nand_output(nand_t const *g, ssize_t k) {
    list_t *current = g->dummyOutputGatesList->next;
    ssize_t counter = 0;

    while (current != NULL && counter != k) {
        counter++;
        current = current->next;
    }

    if (counter == k) {
        return current->gate;
    } else return NULL;
}
