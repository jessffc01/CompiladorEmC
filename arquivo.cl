-- Comentario

class Arvore{
    fotossintese(hora: Int): Bool {
        if 19 <= hora then false else
        if hora < 6 then false else true
        fi fi
    };
};

class Main inherits IO{
    main(): Object{
        (let
        tree: Arvore  <- (new Arvore),
        hora: Int <- in_int(),
        fot: Boolean <- tree.fotossintese(hora)
        in {
        if fot 
        then out_string("Sol presente, hora da fotossintese.\n") 
        else out_string("Sem sol, sem fotossintese.\n")
        fi;
        x <- 10 + 5; -- Soma
        msg <- "resultado";
        (* y <- 0 + 15;
         a <- 2; 
        msg <- "result" *)
        out_string("Teste Comentario.\n");
        } )
    }; 
};