module github.com/Crdzbird/Aviary/bridge/bindings/go

go 1.24

require github.com/Crdzbird/CryptoLib/bridge/bindings/go v0.0.0-00010101000000-000000000000

// cryptolib is not published yet — consume it from the sibling checkout.
replace github.com/Crdzbird/CryptoLib/bridge/bindings/go => ../../../../cryptolib/bridge/bindings/go
