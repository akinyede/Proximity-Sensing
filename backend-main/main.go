package main

import (
	"context"

	"fmt"
	"net/http"

	"github.com/gorilla/handlers"
	"github.com/gorilla/mux"

	"gorm.io/gorm"

	"go.mongodb.org/mongo-driver/mongo"
	"go.mongodb.org/mongo-driver/mongo/options"

	"gorm.io/datatypes"
	// "gorm.io/driver/sqlite"
	// "gorm.io/gorm"
	// "log"
)

// package level variable
var dbName = "rssidata"

var devices_colletion = "devices_colletion"

// var settings_collection = "settings_collection"

//var mongoURI = "mongodb+srv://dandisolutions09:GAFmLYi25DojAuM1@tms-cluster.cp6qfmq.mongodb.net/"
var mongoURI = "mongodb+srv://oluwashayomi:eniola1990@cluster0.kcef1.mongodb.net/"
var client *mongo.Client

type NurseData struct {
	gorm.Model
	Name               string         `json:"name"`
	Department         string         `json:"department"`
	Wards              datatypes.JSON `json:"wards"`
	Grade              string         `json:"grade"`
	SignatureImageData string         `json:"signatureImageData"`
}

func init() {

	var err error

	// Set up MongoDB client
	clientOptions := options.Client().ApplyURI(mongoURI)

	client, err = mongo.Connect(context.Background(), clientOptions)
	if err != nil {
		fmt.Println("Error connecting to MongoDB:", err)
		return
	}

	// Check the connection
	err = client.Ping(context.Background(), nil)
	if err != nil {
		fmt.Println("Error pinging MongoDB:", err)
		return
	}

	fmt.Println("Connected to MongoDB")
}

// Define a struct for your data (adjust fields as needed)
func main() {

	router := mux.NewRouter()

	// Use the handlers.CORS middleware to handle CORS
	corsHandler := handlers.CORS(
		handlers.AllowedHeaders([]string{"Content-Type", "Authorization"}),
		handlers.AllowedMethods([]string{"GET", "HEAD", "POST", "PUT", "DELETE", "OPTIONS"}),
		handlers.AllowedOrigins([]string{"*"}),
	)

	// Attach the CORS middleware to your router

	//FACILITY ROUTES

	router.HandleFunc("/add-logs", handleAddDeviceInfo).Methods("POST")
	router.HandleFunc("/get-data", handleGetDevices).Methods("GET")

	// Start the server on port 8080
	http.Handle("/", corsHandler(router))
	fmt.Println("Server listening on :8080")
	http.ListenAndServe(":8080", nil)
}
