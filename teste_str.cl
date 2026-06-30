class Main inherits IO {
  main() : Object {
    let a : String <- "Compilador",
        b : String <- " COOL",
        c : String <- a.concat(b) in
    {
      out_string(c);
      out_string("\n");

      out_string(c.substr(11, 4));
      out_string("\n");

      0;
    }
  };
};