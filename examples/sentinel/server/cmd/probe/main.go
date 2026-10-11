// probe - a throwaway client that exercises the StreamData bidi RPC end-to-end.
package main

import (
	"context"
	"crypto/rand"
	"fmt"
	"log"
	"time"

	pb "sentinel/gen"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
)

func main() {
	conn, err := grpc.NewClient("127.0.0.1:50052", grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		log.Fatal(err)
	}
	defer conn.Close()
	c := pb.NewSentinelClient(conn)
	ctx, cancel := context.WithTimeout(context.Background(), 25*time.Second)
	defer cancel()

	prov, err := c.ProvisionDevice(ctx, &pb.DeviceInfo{
		DeviceId: "PROBE-DEV", Model: "Pixel 9", Platform: "android",
		OsVersion: "15", Label: "field sensor", Credential: []byte("probe-cred"),
	})
	if err != nil {
		log.Fatal(err)
	}
	fmt.Printf("provisioned chain %d (genesis %s...)\n", prov.ChainId, prov.GenesisHash[:12])

	stream, err := c.StreamData(ctx)
	if err != nil {
		log.Fatal(err)
	}
	done := make(chan string, 1)
	go func() {
		var lastHash string
		for {
			r, err := stream.Recv()
			if err != nil {
				done <- lastHash
				return
			}
			state := "pending"
			if r.Finalized {
				state = fmt.Sprintf("FINALIZED@%d", r.Height)
			}
			fmt.Printf("  receipt seq=%d %-8s %-10s %s... - %s\n", r.Sequence, r.Kind, state, r.ContentHash[:12], r.Summary)
			if r.ContentHash != "" {
				lastHash = r.ContentHash
			}
		}
	}()

	photo := make([]byte, 4096)
	rand.Read(photo)
	now := time.Now().UnixMilli()
	records := []*pb.DataRecord{
		{ChainId: prov.ChainId, Kind: pb.DataKind_PHOTO, Payload: photo, Mime: "image/jpeg", CapturedUnixMillis: now},
		{ChainId: prov.ChainId, Kind: pb.DataKind_LOCATION, Lat: 37.7749, Lon: -122.4194, AccuracyM: 5, CapturedUnixMillis: now + 100},
		{ChainId: prov.ChainId, Kind: pb.DataKind_SENSOR, Sensors: map[string]float64{"temp_c": 21.4, "accel_g": 1.02}, CapturedUnixMillis: now + 200},
	}
	for _, r := range records {
		if err := stream.Send(r); err != nil {
			log.Fatal(err)
		}
		time.Sleep(200 * time.Millisecond)
	}
	time.Sleep(2 * time.Second) // let the sealer finalize
	stream.CloseSend()
	lastHash := <-done

	// Independently verify the last anchored record.
	v, err := c.VerifyRecord(context.Background(), &pb.VerifyRequest{ChainId: prov.ChainId, ContentHash: lastHash})
	if err != nil {
		log.Fatal(err)
	}
	fmt.Printf("verify %s... -> anchored=%v verified=%v height=%d (%d-byte proof)\n",
		lastHash[:12], v.Anchored, v.Verified, v.Height, v.ProofBytes)
}
