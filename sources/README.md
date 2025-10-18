Tema realizata de Barsan Clara, 324CC

Inspiratie:
   * Tema se bazeaza pe laboratorul 9. De acolo am luat strutura organizarii fisierelor, dar si
    functiile specifice HTTP (GET, POST), pe care le-am implementat in timpul laboratorului.
    Totusi, functiile respective au primit adaugate headere (am considerat ca acest lucru este
    necesar pentru a trimite token-ul JWT), iar functiile DELETE si PUT se bazeaza pe cele scrise
    la laborator: GET, respectiv POST si modificate si particularizate ulterior de mine.
    
   * In enuntul temei s-a sugerat sa folosim JSON, lucru pe care l-am si facut. Am ales scrierea
    codului in C (pentru ca Cpp nu stiu la un nivel foarte inalt), motiv pentru care am folosit
    parson. (https://github.com/kgabis/parson - fix de aici am luat tot fisierul).
    
   * Am folosit JWT token pentru ca asa ne era sugerat in tema, iar informarea in acest sens am
    facut-o de pe site-ul urmator: https://jwt.io/introduction


Idee tema:
  * Tema a pornit de la gestionarea fiecarui tip de comanda in parte, iar apoi generalizarea
    partilor comune. Astfel, fiecare comanda respecta un algoritm de genul:
        - citirea datelor de intrare de la tastatura
        - verificarea restrictiilor (daca esti logat ca user/admin, daca ai acces token pentru 
        librarie)
        - formatarea pachetului (payload sau introducerea de cookie/token)
        - trimiterea mesajului si primirea raspunsului
        - gestionarea cazului de eroare/satisfacere conform codului de status din raspunsul primit
    
  * In unele cazuri catre server au fost trimise doua requesturi (add_collection - foloseste
    add_movie_collection), care trimit in plus o cerere de adaugare de filme
