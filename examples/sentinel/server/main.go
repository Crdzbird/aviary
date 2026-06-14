// Sentinel — device provenance ledger gRPC server, backed by Blockchainer.
package main

import (
	"log"
	"net"

	bc "github.com/Crdzbird/Blockchainer/bridge/bindings/go/blockchainer"
	pb "sentinel/gen"

	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"
)

func main() {
	if err := bc.Init(); err != nil {
		log.Fatalf("blockchainer init: %v", err)
	}
	log.Printf("Sentinel device provenance ledger — blockchainer %s (crypto %s)",
		bc.Version(), bc.CryptoBackendVersion())

	addr := ":50052"
	lis, err := net.Listen("tcp", addr)
	if err != nil {
		log.Fatalf("listen: %v", err)
	}
	srv := grpc.NewServer(grpc.MaxRecvMsgSize(32 << 20)) // accept media chunks
	pb.RegisterSentinelServer(srv, newServer())
	reflection.Register(srv)
	log.Printf("listening on %s", addr)
	if err := srv.Serve(lis); err != nil {
		log.Fatalf("serve: %v", err)
	}
}
