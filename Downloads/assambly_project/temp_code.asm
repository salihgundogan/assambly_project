ORG $0100

    LDAA #$0E
    STAA $0200

    INCA
    STAA $0051

    LDAA #$7F
    INCA
    STAA $0053

    LDAA #$FF
    INCA
    STAA $0055

    SWI
    END