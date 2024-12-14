                        beginAtZero: true,
                        title: {
                            display: true,
                            text: 'Glasses'
                        }
                    },
                    x: {
                        title: {
                            display: true,
                            text: 'Time'
                        }
                    }
                }
            }
        };

        // Create the charts
        new Chart(document.getElementById('stepsChart'), stepsConfig);
        new Chart(document.getElementById('waterChart'), waterConfig);
    </script>
</body>
</html>
"""

latest_data = {
    "steps": [],
    "water": []
}

@app.route("/", methods=['GET', 'POST', 'OPTIONS'])
def receive_data():
    global latest_data

    if request.method == 'OPTIONS':
        return '', 200

    if request.method == 'POST':
        if not request.is_json:
            return jsonify({"error": "Content-Type must be application/json"}), 415

        data = request.json
        if not data:
            return jsonify({"error": "Invalid JSON"}), 400

        latest_data['steps'] = data.get("steps", [])
        latest_data['water'] = data.get("water", [])

        return jsonify({"message": "Data received successfully"}), 200

    elif request.method == 'GET':
        if request.headers.get('Accept') == 'application/json' or request.args.get('format') == 'json':
            return jsonify({
                "message": "Current data",
                "data": latest_data
            }), 200
        return render_template_string(HTML_TEMPLATE, data=latest_data)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
~                                                                                                                                              
                                                                                                                             194,13        Bot
