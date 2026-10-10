// Atlas Settlement Network — gRPC server backed by Aviary (Go binding).
package main

import (
	"log"
	"net"

	pb "atlas/gen"
	bc "github.com/Crdzbird/aviary/bridge/bindings/go/aviary"

	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"
)

func main() {
	if err := bc.Init(); err != nil {
		log.Fatalf("aviary init: %v", err)
	}
	log.Printf("Atlas Settlement Network — aviary %s (crypto %s)",
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
