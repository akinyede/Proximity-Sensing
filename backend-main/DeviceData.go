package main

import (
	// "container/list"
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	// "time"

	// "time"

	// "time"

	//"github.com/gorilla/handlers"
	// "github.com/gorilla/mux"
	// "go.mongodb.org/mongo-driver/bson"
	// "github.com/gorilla/mux"
	"go.mongodb.org/mongo-driver/bson"
	"go.mongodb.org/mongo-driver/bson/primitive"
	// "gorm.io/gorm"
	// "go.mongodb.org/mongo-driver/bson/primitive"
	//"go.mongodb.org/mongo-driver/mongo"
	//"go.mongodb.org/mongo-driver/mongo/options"
)

// const ongoing_json = {
// 	distance: postData.distance,
// 	rssi: postData.rssi,
// 	name: postData.deviceAddress,
// 	created_at: new Date().toLocaleString(),
//   };

type Device struct {
	ID        primitive.ObjectID `json:"_id,omitempty" bson:"_id,omitempty"`
	Name      string             `json:"device_name" bson:"device_name"`
	Distance  string             `json:"distance" bson:"distance"`
	Rssi      string             `json:"rssi" bson:"rssi"`
	Bridge    string             `json:"bridge" bson:"bridge"`
	CreatedAt string             `json:"created_at" bson:"created_at"`
}

func handleAddDeviceInfo(w http.ResponseWriter, r *http.Request) {
	// Parse JSON data from the request
	fmt.Println("incoming data:", r.Body)

	decoder := json.NewDecoder(r.Body)
	var data Device
	err := decoder.Decode(&data)
	if err != nil {
		http.Error(w, "Error decoding JSON", http.StatusBadRequest)
		return
	}

	fmt.Println("data", data)

	// Insert data into MongoDB
	collection := client.Database(dbName).Collection(devices_colletion)
	_, err = collection.InsertOne(context.Background(), data)
	fmt.Print(data)
	if err != nil {
		http.Error(w, "Error inserting nurse", http.StatusInternalServerError)
		return
	}

	//	Respond with a success message
	fmt.Fprintf(w, "Device inserted Successfully")

}

//GET FACILITY BY ID

//GET ALL FACILITIES

func handleGetDevices(w http.ResponseWriter, r *http.Request) {
	collection := client.Database(dbName).Collection(devices_colletion)

	// Define a slice to store retrieved facilities
	var devices []Device

	// Retrieve all documents from the collection
	cursor, err := collection.Find(context.Background(), bson.D{})
	if err != nil {
		http.Error(w, "Error retrieving devices", http.StatusInternalServerError)
		return
	}
	defer cursor.Close(context.Background())

	// Iterate through the cursor and decode documents into the facilities slice
	for cursor.Next(context.Background()) {
		var device Device
		if err := cursor.Decode(&device); err != nil {
			http.Error(w, "Error decoding device", http.StatusInternalServerError)
			return
		}
		devices = append(devices, device)
	}

	// Respond with the retrieved facilities in JSON format
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(devices)
}
