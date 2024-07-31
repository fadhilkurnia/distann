# in order to run: flask --app (filename) run

import os

import numpy as np
import torch
from flask import Flask, jsonify, request
from PIL import Image
from transformers import CLIPModel, CLIPProcessor

app = Flask(__name__)


model_name = "openai/clip-vit-base-patch32"
model = CLIPModel.from_pretrained(model_name)
processor = CLIPProcessor.from_pretrained(model_name)


def vectorize_text(text):
    inputs = processor(text, return_tensors="pt")
    with torch.no_grad():
        text_features = model.get_text_features(**inputs)
    return text_features


@app.route("/embed", methods=["POST"])
def embed():
    try:
        data = request.json
        query = data.get("query", "")

        if not query:
            return jsonify({"error": "No Query Provided"}), 400

        embedding = vectorize_text(query)
        embedding = embedding.squeeze().tolist()
        return jsonify({"embedding": embedding})

    except Exception as e:
        return jsonify({"error": str(e)}), 500
