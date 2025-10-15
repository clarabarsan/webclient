# Python + Flask makefile

VENV = .venv
VENV_PYTHON3 = $(VENV)/bin/python3

ADMIN ?= test:testpass
PROGRAM ?= ../client

all: venv deps

venv: $(VENV_PYTHON3)
$(VENV_PYTHON3):
	python3 -m venv "$(VENV)"

deps: venv
	$(VENV_PYTHON3) -m pip install -r requirements.txt

A ?= --debug --admin "$(ADMIN)"
run:
	$(VENV_PYTHON3) checker.py $(PROGRAM) $(A)


#CC=gcc
#CFLAGS=-I.

#client: client.c requests.c helpers.c buffer.c
#	$(CC) -o client client.c requests.c helpers.c buffer.c -Wall

#run: client
#	./client

#clean:
#	rm -f *.o client


