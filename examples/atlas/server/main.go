// Atlas Settlement Network — gRPC server backed by Blockchainer (Go binding).
package main

import (
	"log"
	"net"

	pb "atlas/gen"
	bc "github.com/Crdzbird/Blockchainer/bridge/bindings/go/blockchainer"

	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"
)

func main() {
	if err := bc.Init(); err != nil {
		log.Fatalf("blockchainer init: %v", err)
	}
	log.Printf("Atlas Settlement Network — blockchainer %s (crypto %s)",
		bc.Version(), bc.CryptoBackendVersion())

	addr := ":50051"
	lis, err := net.Listen("tcp", addr)
	if err != nil {
		log.Fatalf("listen: %v", err)
	}
	grpcServer := grpc.NewServer()
	pb.RegisterAtlasServer(grpcServer, newServer())
	reflection.Register(grpcServer) // enables grpcurl / Flutter discovery
	log.Printf("listening on %s", addr)
	if err := grpcServer.Serve(lis); err != nil {
		log.Fatalf("serve: %v", err)
	}
}
