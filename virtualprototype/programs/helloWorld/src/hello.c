#include <stdio.h>
#include <vga.h>
#include <spr.h>
// (Placez la fonction utoa juste au-dessus)

unsigned int utoa(unsigned int number, char *buf, unsigned int bufsz, unsigned int base, const char *digits) {
    // Contraintes de sécurité initiales
    if (bufsz <= 1 || base <= 1) {
        if (bufsz > 0) buf[0] = '\0';
        return 0;
    }

    unsigned int i = 0;

    // Cas particulier : le nombre est 0
    if (number == 0) {
        buf[i++] = digits[0];
    } else {
        // Extraction des caractères (générés à l'envers)
        while (number > 0) {
            // Vérification anti-débordement (buffer overflow)
            if (i >= bufsz - 1) {
                buf[0] = '\0';
                return 0; // Échec
            }
            buf[i++] = digits[number % base];
            number /= base;
        }
    }

    // Ajout du caractère de fin de chaîne obligatoire
    buf[i] = '\0';

    // Inversion de la chaîne de caractères
    unsigned int start = 0;
    unsigned int end = i - 1;
    while (start < end) {
        char temp = buf[start];
        buf[start] = buf[end];
        buf[end] = temp;
        start++;
        end--;
    }

    return i; // Retourne le nombre de caractères écrits
}

int main() {
    const char *vigesimal_digits = "0123456789ABCDEFGHIJ";
    char buffer[32]; // Un buffer de 32 caractères est largement suffisant

    for (unsigned int n = 0; n <= 100; n++) {
        // Si la conversion réussit (retour > 0), on affiche le résultat
        if (utoa(n, buffer, sizeof(buffer), 20, vigesimal_digits) > 0) {
            // REMARQUE : Adaptez printf à la fonction d'affichage de votre TP
            // S'il s'agit d'une maquette virtuelle, ce sera peut-être uart_print() ou similaire
            printf("%s\n", buffer); 
        }
    }

    return 0;
}


