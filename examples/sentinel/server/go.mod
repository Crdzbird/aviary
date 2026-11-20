module sentinel

go 1.26.0

require (
	github.com/Crdzbird/aviary/bridge/bindings/go v0.0.0-00010101000000-000000000000
	google.golang.org/grpc v1.85.0
	google.golang.org/protobuf v1.36.12
)

require (
	github.com/Crdzbird/CipherBird/bridge/bindings/go v0.0.0-20261120163107-23735fcf3b48 // indirect
	golang.org/x/net v0.60.0 // indirect
	golang.org/x/sys v0.48.0 // indirect
	golang.org/x/text v0.42.0 // indirect
	google.golang.org/genproto/googleapis/rpc v0.0.0-20260817212433-ac3dfec99bb1 // indirect
)

// Local sibling checkouts (neither published yet).
replace github.com/Crdzbird/aviary/bridge/bindings/go => ../../../bridge/bindings/go
