module sentinel

go 1.24

require (
	github.com/Crdzbird/aviary/bridge/bindings/go v0.0.0-00010101000000-000000000000
	google.golang.org/grpc v1.68.0
	google.golang.org/protobuf v1.35.2
)

require (
	github.com/Crdzbird/CipherBird/bridge/bindings/go v0.0.0-20261005192042-bffafd77df2f // indirect
	golang.org/x/net v0.29.0 // indirect
	golang.org/x/sys v0.25.0 // indirect
	golang.org/x/text v0.18.0 // indirect
	google.golang.org/genproto/googleapis/rpc v0.0.0-20240903143218-8af14fe29dc1 // indirect
)

// Local sibling checkouts (neither published yet).
replace github.com/Crdzbird/aviary/bridge/bindings/go => ../../../bridge/bindings/go
